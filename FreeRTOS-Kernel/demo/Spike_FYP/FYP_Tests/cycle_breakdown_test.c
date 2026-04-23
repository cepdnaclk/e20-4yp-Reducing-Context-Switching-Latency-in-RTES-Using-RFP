/*
 * Phase 1: Cycle-Accurate Latency Breakdown Test
 *
 * Measures exact cycles for:
 * - Baseline (legacy): push 31 regs, scheduler, pop 31 regs (save_cycles, restore_cycles).
 * - Custom (windowed): write 0x801 + minimal save/restore (minimal_save_cycles, minimal_restore_cycles).
 *
 * Build with -DCYCLE_BREAKDOWN_PROFILE; link with port_cycle_profile.o and portASM_cycle_profile.o.
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#if ( configUSE_RISCV_REGISTER_WINDOWS != 1 )
    #error Build with configUSE_RISCV_REGISTER_WINDOWS=1
#endif

#ifndef CYCLE_BREAKDOWN_PROFILE
    #error Build with -DCYCLE_BREAKDOWN_PROFILE
#endif

#define UART0         ((volatile char *)0x10000000)
#define SAMPLES       200

extern volatile uint32_t port_profile_save_begin;
extern volatile uint32_t port_profile_save_end;
extern volatile uint32_t port_profile_minimal_save_begin;
extern volatile uint32_t port_profile_minimal_save_end;
extern volatile uint32_t port_profile_restore_begin;
extern volatile uint32_t port_profile_restore_end;
extern volatile uint32_t port_profile_minimal_restore_begin;
extern volatile uint32_t port_profile_minimal_restore_end;
extern volatile uint32_t port_profile_minimal_restore_taken;

static void putstr( const char * s ) { while ( *s ) { *UART0 = *s++; } }

static void putdec( uint32_t x )
{
    char buf[12];
    char * p = buf + sizeof(buf) - 1;
    *p = '\0';
    if ( x == 0 ) { *(--p) = '0'; }
    while ( x ) { *(--p) = ( char )( '0' + ( x % 10 ) ); x /= 10; }
    putstr( p );
}

static void puthex( uint32_t x )
{
    const char hex[] = "0123456789abcdef";
    char buf[11];
    int i;
    buf[0] = '0';
    buf[1] = 'x';
    for ( i = 9; i >= 2; i-- ) { buf[i] = hex[x & 0xF]; x >>= 4; }
    buf[10] = '\0';
    putstr( buf );
}

static uint32_t save_cycles[SAMPLES], restore_cycles[SAMPLES];
static uint32_t minimal_save_cycles[SAMPLES], minimal_restore_cycles[SAMPLES];
static volatile uint32_t sample_idx_a;   /* full path (task A) */
static volatile uint32_t sample_idx_b;   /* minimal path (task B) */
static volatile uint32_t minimal_restore_path_ever_taken;   /* set by B when it sees port_profile_minimal_restore_taken */
static volatile uint32_t phase_done;
static volatile uint32_t printed;

static void print_stats_full( const char * label, uint32_t * arr, uint32_t n )
{
    uint32_t i, sum = 0, min = 0xFFFFFFFFU, max = 0;
    for ( i = 0; i < n; i++ )
    {
        uint32_t v = arr[ i ];
        sum += v;
        if ( v < min ) min = v;
        if ( v > max ) max = v;
    }
    putstr( label );
    putstr( "  min=" ); putdec( min );
    putstr( " max=" ); putdec( max );
    putstr( " avg=" ); putdec( n ? ( sum / n ) : 0 );
    putstr( " cycles\r\n" );
}

void vTaskBreakdownA( void * pv )
{
    static uint8_t first_a = 1;
    ( void ) pv;
    for ( ; ; )
    {
        if ( first_a ) { first_a = 0; putstr( "A" ); }
        if ( sample_idx_a >= SAMPLES )
        {
            if ( sample_idx_b >= SAMPLES ) phase_done = 1;
            vTaskSuspend( NULL );
            continue;
        }
        taskYIELD();
        /* We resumed: restore just finished (full path). Save was our full save. */
        if ( sample_idx_a < SAMPLES )
        {
            save_cycles[ sample_idx_a ] = port_profile_save_end - port_profile_save_begin;
            restore_cycles[ sample_idx_a ] = port_profile_restore_end - port_profile_restore_begin;
            sample_idx_a++;
            if ( ( sample_idx_a % 50 ) == 0 && sample_idx_a > 0 )
                putstr( "." );
        }
        taskYIELD();
    }
}

void vTaskBreakdownB( void * pv )
{
    static uint8_t first_b = 1;
    ( void ) pv;
    for ( ; ; )
    {
        if ( first_b ) { first_b = 0; putstr( "B" ); }
        if ( sample_idx_b >= SAMPLES )
        {
            if ( sample_idx_a >= SAMPLES ) phase_done = 1;
            vTaskSuspend( NULL );
            continue;
        }
        taskYIELD();
        /* We resumed: record every time (first restore to B may be full path so first sample can be 0; stats skip it). */
        if ( sample_idx_b < SAMPLES )
        {
            if ( port_profile_minimal_restore_taken )
                minimal_restore_path_ever_taken = 1;
            minimal_save_cycles[ sample_idx_b ] = port_profile_minimal_save_end - port_profile_minimal_save_begin;
            minimal_restore_cycles[ sample_idx_b ] = port_profile_minimal_restore_end - port_profile_minimal_restore_begin;
            sample_idx_b++;
            if ( ( sample_idx_b % 50 ) == 0 && sample_idx_b > 0 )
                putstr( "." );
        }
        taskYIELD();
    }
}

void vApplicationIdleHook( void )
{
    if ( phase_done && !printed )
    {
        printed = 1;
        putstr( "\r\n--- Cycle breakdown ---\r\n" );
        putstr( "Full path (push 31 regs, pop 31 regs):\r\n" );
        print_stats_full( "  Save:    ", save_cycles, sample_idx_a );
        print_stats_full( "  Restore: ", restore_cycles, sample_idx_a );
        putstr( "Minimal path (0x801 + mret):\r\n" );
        putstr( "  Minimal restore path taken: " );
        putstr( minimal_restore_path_ever_taken ? "yes" : "no" );
        putstr( "\r\n" );
        /* Minimal restore: globals are correct when idle runs (last minimal restore). Per-sample can be 0 if B reads before write. */
        putstr( "  Restore: " );
        putdec( port_profile_minimal_restore_end - port_profile_minimal_restore_begin );
        putstr( " cycles\r\n" );
        if ( sample_idx_b > 1 )
            print_stats_full( "  Save:    ", minimal_save_cycles + 1, sample_idx_b - 1 );
        else
            putstr( "  Save:    (no minimal samples)\r\n" );
    }
}

void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    size_t * pulIdleTaskStackSize )
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = ( size_t ) configMINIMAL_STACK_SIZE;
}

int main( void )
{
    putstr( "\r\n=== Phase 1: Cycle-Accurate Latency Breakdown ===\r\n" );
    putstr( "Collecting " ); putdec( SAMPLES ); putstr( " samples (A=full, B=minimal)...\r\n" );

    sample_idx_a = 0;
    sample_idx_b = 0;
    phase_done = 0;
    printed = 0;

    static StackType_t xStackA[ configMINIMAL_STACK_SIZE ];
    static StackType_t xStackB[ configMINIMAL_STACK_SIZE ];
    static StaticTask_t xTCBA, xTCBB;
    TaskHandle_t xB;

    ( void ) xTaskCreateStatic( vTaskBreakdownA, "A", configMINIMAL_STACK_SIZE, NULL, 1, xStackA, &xTCBA );
    xB = xTaskCreateStatic( vTaskBreakdownB, "B", configMINIMAL_STACK_SIZE, NULL, 1, xStackB, &xTCBB );
    if ( xB != NULL )
        vPortTaskSetRegisterWindow( xB, 32, 32 );
    ( void ) xB;

    putstr( "Starting scheduler...\r\n" );
    vTaskStartScheduler();
    for ( ; ; ) { }
    return 0;
}
