/*
 * Phase 3: WCET & Jitter Test
 *
 * Round-trip latency = mcycle at yield to mcycle when task runs again (includes
 * time the other task and the flood task run). So under flood, A and B can show
 * similar min/max/avg because the dominant delay is waiting for the scheduler,
 * not the context-switch cost itself.
 *
 * How to run for the FYP:
 *   - No flood: make demo_jitter_noflood.elf && spike --isa=rv32gc demo_jitter_noflood.elf
 *     Expect A~342, B~163 (clear gap). Use SAMPLES 16–100; disable tick in
 *     FreeRTOSConfig.h for cleanest comparison.
 *   - With flood: make demo_jitter.elf && spike --isa=rv32gc demo_jitter.elf
 *     Round-trip includes time the flood task runs, so A and B often show similar
 *     min/max/avg; use SAMPLES 50–100. Report shows "Round-trip latency (flood
 *     task at prio 0)". Main result for the paper: no-flood shows the latency gap.
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#if ( configUSE_RISCV_REGISTER_WINDOWS != 1 )
    #error Build with configUSE_RISCV_REGISTER_WINDOWS=1
#endif

#define UART0         ((volatile char *)0x10000000)
#define SAMPLES       16    /* use 50/100/300/1000; running min/max/sum so no large arrays */
#define FLOOD_SIZE    256   /* words written by flood task per loop */
#define JITTER_STACK_WORDS  256   /* larger stack for latency tasks (avoid overflow) */
/* Define FYP_JITTER_NO_FLOOD to skip flood task (quick sanity run). */

/* Running stats: O(1) RAM so any SAMPLES (e.g. 300, 1000) works without .bss overflow. */
static volatile uint32_t min_a, max_a, sum_a;
static volatile uint32_t min_b, max_b, sum_b;
static volatile uint32_t sample_idx_a;
static volatile uint32_t sample_idx_b;
static volatile uint32_t phase_done;
static volatile uint32_t tasks_done;  /* 0=none, 1=first task printed header+own line, 2=both done */

/* Memory flood buffer (in .bss so it uses RAM and causes bus traffic when written). */
static volatile uint32_t flood_buf[FLOOD_SIZE];

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

/* Inlined round-trip: no extra call frame across yield (same pattern as latency_test.c). */

/* Print one line from precomputed min/max/sum/n (no array, works for any SAMPLES). */
static void print_stats_line( const char * label, uint32_t min_val, uint32_t max_val, uint32_t sum, uint32_t n )
{
    if ( n == 0 ) { putstr( label ); putstr( " (no samples)\r\n" ); return; }
    putstr( label );
    putstr( "  min=" ); putdec( min_val );
    putstr( " max=" ); putdec( max_val );
    putstr( " avg=" ); putdec( sum / n );
    putstr( " jitter=" ); putdec( max_val - min_val );
    putstr( " cycles\r\n" );
}

void vTaskLatencyA( void * pv )
{
    static uint8_t first = 1;
    uint32_t t_start, t_end;
    ( void ) pv;
    for ( ; ; )
    {
        if ( first ) { first = 0; putstr( "A" ); }
        else { putstr( "." ); }
        if ( sample_idx_a >= SAMPLES )
        {
            phase_done = 1;
            if ( tasks_done == 0 )
                tasks_done = 1;  /* first to finish: just mark, no print */
            else
            {
                /* second to finish: print full report once (no duplicate, no cross-task read) */
                tasks_done = 2;
#ifdef FYP_JITTER_NO_FLOOD
                putstr( "\r\n--- Phase 3: Jitter (no flood) ---\r\nRound-trip latency:\r\n" );
#else
                putstr( "\r\n--- Phase 3: Jitter under memory load ---\r\nRound-trip latency (flood task at prio 0):\r\n" );
#endif
                print_stats_line( "  A (legacy):   ", min_a, max_a, sum_a, sample_idx_a );
                print_stats_line( "  B (windowed): ", min_b, max_b, sum_b, sample_idx_b );
            }
            vTaskSuspend( NULL );
            continue;
        }
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_start ) );
        taskYIELD();
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_end ) );
        if ( sample_idx_a < SAMPLES )
        {
            uint32_t d = t_end - t_start;
            if ( sample_idx_a == 0 ) { min_a = max_a = d; sum_a = d; }
            else { if ( d < min_a ) min_a = d; if ( d > max_a ) max_a = d; sum_a += d; }
            sample_idx_a++;
        }
        taskYIELD();
    }
}

void vTaskLatencyB( void * pv )
{
    static uint8_t first = 1;
    uint32_t t_start, t_end;
    ( void ) pv;
    for ( ; ; )
    {
        if ( first ) { first = 0; putstr( "B" ); }
        else { putstr( "." ); }
        if ( sample_idx_b >= SAMPLES )
        {
            phase_done = 1;
            if ( tasks_done == 0 )
                tasks_done = 1;  /* first to finish: just mark, no print */
            else
            {
                /* second to finish: print full report once (no duplicate, no cross-task read) */
                tasks_done = 2;
#ifdef FYP_JITTER_NO_FLOOD
                putstr( "\r\n--- Phase 3: Jitter (no flood) ---\r\nRound-trip latency:\r\n" );
#else
                putstr( "\r\n--- Phase 3: Jitter under memory load ---\r\nRound-trip latency (flood task at prio 0):\r\n" );
#endif
                print_stats_line( "  A (legacy):   ", min_a, max_a, sum_a, sample_idx_a );
                print_stats_line( "  B (windowed): ", min_b, max_b, sum_b, sample_idx_b );
            }
            vTaskSuspend( NULL );
            continue;
        }
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_start ) );
        taskYIELD();
        __asm volatile ( "csrr %0, mcycle" : "=r"( t_end ) );
        if ( sample_idx_b < SAMPLES )
        {
            uint32_t d = t_end - t_start;
            if ( sample_idx_b == 0 ) { min_b = max_b = d; sum_b = d; }
            else { if ( d < min_b ) min_b = d; if ( d > max_b ) max_b = d; sum_b += d; }
            sample_idx_b++;
        }
        taskYIELD();
    }
}

/* Background task: flood memory to create bus contention. */
void vTaskFlood( void * pv )
{
    ( void ) pv;
    for ( ; ; )
    {
        for ( int i = 0; i < FLOOD_SIZE; i++ )
            flood_buf[ i ] = ( uint32_t ) i;
        taskYIELD();
    }
}

void vApplicationIdleHook( void )
{
    ( void ) phase_done;  /* report is printed by the tasks when they suspend */
}

#define IDLE_STACK_WORDS  256   /* larger stack for report printing (putdec, etc.) under flood */
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    size_t * pulIdleTaskStackSize )
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ IDLE_STACK_WORDS ];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = ( size_t ) IDLE_STACK_WORDS;
}

int main( void )
{
    putstr( "\r\n=== Phase 3: WCET & Jitter ===\r\n" );
#ifdef FYP_JITTER_NO_FLOOD
    putstr( "Tasks: A (legacy), B (windowed), no flood. " );
#else
    putstr( "Tasks: A (legacy), B (windowed), Flood (prio 0). " );
#endif
    putdec( SAMPLES );
    putstr( " round-trip samples.\r\n" );

    sample_idx_a = 0;
    sample_idx_b = 0;
    phase_done = 0;
    tasks_done = 0;

    static StackType_t xStackA[ JITTER_STACK_WORDS ];
    static StackType_t xStackB[ JITTER_STACK_WORDS ];
#ifndef FYP_JITTER_NO_FLOOD
    static StackType_t xStackFlood[ configMINIMAL_STACK_SIZE ];
    static StaticTask_t xTCBFlood;
#endif
    static StaticTask_t xTCBA, xTCBB;
    TaskHandle_t xB;

    ( void ) xTaskCreateStatic( vTaskLatencyA, "A", JITTER_STACK_WORDS, NULL, 1, xStackA, &xTCBA );
    xB = xTaskCreateStatic( vTaskLatencyB, "B", JITTER_STACK_WORDS, NULL, 1, xStackB, &xTCBB );
    if ( xB != NULL )
        vPortTaskSetRegisterWindow( xB, 32, 32 );
    ( void ) xB;
#ifndef FYP_JITTER_NO_FLOOD
    ( void ) xTaskCreateStatic( vTaskFlood, "Flood", configMINIMAL_STACK_SIZE, NULL, 0, xStackFlood, &xTCBFlood );
#endif

    putstr( "Starting scheduler...\r\n" );
    vTaskStartScheduler();
    for ( ; ; ) { }
    return 0;
}
