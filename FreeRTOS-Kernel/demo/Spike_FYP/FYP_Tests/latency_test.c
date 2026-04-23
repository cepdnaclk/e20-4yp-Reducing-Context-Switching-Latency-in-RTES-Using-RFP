/*
 * FYP Latency Test: measure context-switch round-trip cycles (mcycle).
 * Build normally for "Legacy vs Windowed" (A legacy, B windowed).
 * Build with -DFYP_LATENCY_LEGACY_ONLY for "Legacy vs Legacy" (baseline).
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#if ( configUSE_RISCV_REGISTER_WINDOWS != 1 )
    #error Build with configUSE_RISCV_REGISTER_WINDOWS=1
#endif

#define UART0         ((volatile char *)0x10000000)
#define SAMPLES       1000
#define WARMUP        10

static volatile uint32_t latencies[SAMPLES];
static volatile uint32_t sample_idx;
static volatile uint32_t phase_done;
static volatile uint32_t printed;
#ifndef FYP_LATENCY_LEGACY_ONLY
/* Window persistence check: windowed task B sets s0 to MAGIC before yield and reads after; if equal, minimal path kept B's window. */
static volatile uint32_t window_check_s0_after_yield;
#define WINDOW_MAGIC 0xDEADBEEFU
#endif

static void putstr( const char * s )
{
    while ( *s ) { *UART0 = *s++; }
}

static void putdec( uint32_t x )
{
    char buf[12];
    char * p = buf + sizeof(buf) - 1;
    *p = '\0';
    if ( x == 0 ) { *(--p) = '0'; }
    while ( x ) { *(--p) = ( char )( '0' + ( x % 10 ) ); x /= 10; }
    putstr( p );
}

static void print_stats( void )
{
    uint32_t i, sum = 0, min = 0xFFFFFFFFU, max = 0;
    for ( i = WARMUP; i < SAMPLES; i++ )
    {
        uint32_t v = latencies[ i ];
        sum += v;
        if ( v < min ) min = v;
        if ( v > max ) max = v;
    }
    if ( SAMPLES > WARMUP )
    {
        putstr( "  Min: " ); putdec( min ); putstr( " cycles\r\n" );
        putstr( "  Max: " ); putdec( max ); putstr( " cycles\r\n" );
        putstr( "  Avg: " ); putdec( sum / ( SAMPLES - WARMUP ) ); putstr( " cycles\r\n" );
    }
#ifndef FYP_LATENCY_LEGACY_ONLY
    putstr( "  Window check: " );
    if ( window_check_s0_after_yield == WINDOW_MAGIC )
        putstr( "s0 persisted (windowed path OK)\r\n" );
    else
        putstr( "s0 lost (check MRET applies 0x801 in sim)\r\n" );
#endif
}

void vTaskLatencyA( void * pv )
{
    static uint32_t first_run = 1;
    ( void ) pv;
    uint32_t t_start, t_end;
    for ( ; ; )
    {
        if ( first_run )
        {
            first_run = 0;
            putstr( "." );  /* progress: first task is running */
        }
        if ( sample_idx >= SAMPLES )
        {
            phase_done = 1;
            vTaskSuspend( NULL );  /* so idle can run and print stats */
            continue;
        }
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_start ) );
        taskYIELD();
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_end ) );
        if ( sample_idx < SAMPLES )
        {
            latencies[ sample_idx ] = t_end - t_start;
            sample_idx++;
            if ( sample_idx == SAMPLES && !printed )
            {
                printed = 1;
                print_stats();
            }
            if ( ( sample_idx % 200 ) == 0 && sample_idx > 0 )
                putstr( "." );  /* progress every 200 samples */
        }
        taskYIELD();
    }
}

void vTaskLatencyB( void * pv )
{
    static uint32_t first_b = 1;
#ifndef FYP_LATENCY_LEGACY_ONLY
    static uint32_t window_check_done = 0;  /* Run persistence check once when B has run with sample_idx >= 1 (works whether A or B runs first). */
#endif
    ( void ) pv;
    uint32_t t_start, t_end;
    for ( ; ; )
    {
        if ( first_b ) { first_b = 0; putstr( "B" ); }  /* Windowed task ran; minimal path used when B yields. */
#ifndef FYP_LATENCY_LEGACY_ONLY
        /* Persistence check: once, when sample_idx >= 1, set s0 to MAGIC, yield, then read s0. If window is applied on mret, s0 persists. */
        if ( !window_check_done && sample_idx >= 1 )
        {
            window_check_done = 1;
            uint32_t magic = WINDOW_MAGIC;
            __asm volatile ( "mv s0, %0" : : "r"( magic ) : "s0" );
            taskYIELD();
            __asm volatile ( "sw s0, 0(%0)" : : "r"( &window_check_s0_after_yield ) : "memory" );
        }
#endif
        if ( sample_idx >= SAMPLES )
        {
            phase_done = 1;
            vTaskSuspend( NULL );  /* so idle can run and print stats */
            continue;
        }
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_start ) );
        taskYIELD();
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_end ) );
        if ( sample_idx < SAMPLES )
        {
            latencies[ sample_idx ] = t_end - t_start;
            sample_idx++;
            if ( sample_idx == SAMPLES && !printed )
            {
                printed = 1;
                print_stats();
            }
            if ( ( sample_idx % 200 ) == 0 && sample_idx > 0 )
                putstr( "." );  /* progress every 200 samples */
        }
        taskYIELD();
    }
}

void vApplicationIdleHook( void )
{
    if ( phase_done && !printed )
    {
        printed = 1;
        print_stats();
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
#ifdef FYP_LATENCY_LEGACY_ONLY
    putstr( "\r\n=== FYP Latency Test: Legacy vs Legacy (baseline) ===\r\n" );
#else
    putstr( "\r\n=== FYP Latency Test: Legacy vs Windowed ===\r\n" );
#endif
    putstr( "Collecting " ); putdec( SAMPLES ); putstr( " round-trip samples...\r\n" );

    sample_idx = 0;
    phase_done = 0;
    printed = 0;

    static StackType_t xStackA[ configMINIMAL_STACK_SIZE ];
    static StackType_t xStackB[ configMINIMAL_STACK_SIZE ];
    static StaticTask_t xTCBA, xTCBB;
    TaskHandle_t xB;

    ( void ) xTaskCreateStatic( vTaskLatencyA, "A", configMINIMAL_STACK_SIZE, NULL, 1, xStackA, &xTCBA );
    xB = xTaskCreateStatic( vTaskLatencyB, "B", configMINIMAL_STACK_SIZE, NULL, 1, xStackB, &xTCBB );
#ifndef FYP_LATENCY_LEGACY_ONLY
    if ( xB != NULL )
        vPortTaskSetRegisterWindow( xB, 32, 32 );
#endif
    ( void ) xB;

    putstr( "Starting scheduler...\r\n" );
    vTaskStartScheduler();
    for ( ; ; ) { }
    return 0;
}
