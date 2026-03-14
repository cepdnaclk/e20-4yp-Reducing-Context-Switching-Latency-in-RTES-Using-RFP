/*
 * FYP Workload Test: context-switch latency with workload between yields.
 * Task A: 4x4 matrix multiply then yield; Task B: string reverse then yield.
 * Build with -DFYP_WORKLOAD_LEGACY_ONLY for both legacy (baseline).
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#if ( configUSE_RISCV_REGISTER_WINDOWS != 1 )
    #error Build with configUSE_RISCV_REGISTER_WINDOWS=1
#endif

#define UART0         ((volatile char *)0x10000000)
#define SAMPLES       100
#define WARMUP        5

static volatile uint32_t latencies[SAMPLES];
static volatile uint32_t sample_idx;
static volatile uint32_t phase_done;
static volatile uint32_t printed;

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

static void matrix_mul_4x4( void )
{
    int A[4][4] = { {1,2,3,4}, {5,6,7,8}, {9,1,2,3}, {4,5,6,7} };
    int B[4][4] = { {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1} };
    int C[4][4];
    int i, j, k;
    for ( i = 0; i < 4; i++ )
        for ( j = 0; j < 4; j++ )
            C[i][j] = 0;
    for ( i = 0; i < 4; i++ )
        for ( j = 0; j < 4; j++ )
            for ( k = 0; k < 4; k++ )
                C[i][j] += A[i][k] * B[k][j];
    ( void ) C;
}

static void string_reverse( void )
{
    char str[] = "HardwareAcceleratedRISCV";
    int len = 24;
    int i;
    for ( i = 0; i < len / 2; i++ )
    {
        char t = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = t;
    }
    ( void ) str;
}

static void print_stats( void )
{
    uint32_t i, sum = 0, min = 0xFFFFFFFFU, max = 0;
    for ( i = WARMUP; i < SAMPLES; i++ )
    {
        uint32_t v = latencies[i];
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
}

void vTaskWorkA( void * pv )
{
    ( void ) pv;
    uint32_t t_start, t_end;
    for ( ; ; )
    {
        if ( sample_idx >= SAMPLES )
        {
            phase_done = 1;
            vTaskSuspend( NULL );  /* so idle can run and print stats */
            continue;
        }
        matrix_mul_4x4();
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_start ) );
        taskYIELD();
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_end ) );
        if ( sample_idx < SAMPLES )
        {
            latencies[sample_idx] = t_end - t_start;
            sample_idx++;
            if ( sample_idx == SAMPLES && !printed )
            {
                printed = 1;
                putstr( "Workload test done.\r\n" );
                print_stats();
            }
        }
        taskYIELD();
    }
}

void vTaskWorkB( void * pv )
{
    ( void ) pv;
    uint32_t t_start, t_end;
    for ( ; ; )
    {
        if ( sample_idx >= SAMPLES )
        {
            phase_done = 1;
            vTaskSuspend( NULL );  /* so idle can run and print stats */
            continue;
        }
        string_reverse();
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_start ) );
        taskYIELD();
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_end ) );
        if ( sample_idx < SAMPLES )
        {
            latencies[sample_idx] = t_end - t_start;
            sample_idx++;
            if ( sample_idx == SAMPLES && !printed )
            {
                printed = 1;
                putstr( "Workload test done.\r\n" );
                print_stats();
            }
        }
        taskYIELD();
    }
}

void vApplicationIdleHook( void )
{
    if ( phase_done && !printed )
    {
        printed = 1;
        putstr( "Workload test done.\r\n" );
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
#ifdef FYP_WORKLOAD_LEGACY_ONLY
    putstr( "\r\n=== FYP Workload Test (Legacy vs Legacy) ===\r\n" );
#else
    putstr( "\r\n=== FYP Workload Test (Legacy vs Windowed + workload) ===\r\n" );
#endif
    putstr( "Samples: " ); putdec( SAMPLES ); putstr( " (matrix + string workload).\r\n" );

    sample_idx = 0;
    phase_done = 0;
    printed = 0;

    static StackType_t xStackA[ configMINIMAL_STACK_SIZE ];
    static StackType_t xStackB[ configMINIMAL_STACK_SIZE ];
    static StaticTask_t xTCBA, xTCBB;
    TaskHandle_t xB;

    ( void ) xTaskCreateStatic( vTaskWorkA, "A", configMINIMAL_STACK_SIZE, NULL, 1, xStackA, &xTCBA );
    xB = xTaskCreateStatic( vTaskWorkB, "B", configMINIMAL_STACK_SIZE, NULL, 1, xStackB, &xTCBB );
#ifndef FYP_WORKLOAD_LEGACY_ONLY
    if ( xB != NULL )
        vPortTaskSetRegisterWindow( xB, 32, 32 );
#endif
    ( void ) xB;

    vTaskStartScheduler();
    for ( ; ; ) { }
    return 0;
}
