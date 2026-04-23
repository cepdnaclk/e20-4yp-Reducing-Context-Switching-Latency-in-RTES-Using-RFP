/*
 * Phase 4: Window Spilling (Scalability) Test
 *
 * Spill: more tasks than hardware windows. Assign a window to the first WINDOWED_TASKS;
 * the rest use full save/restore (graceful degradation). NXPR=256 => 8 windows => set
 * WINDOWED_TASKS=7 (tasks 0..6 windowed, 7..9 legacy). NXPR=64 => 1 window => set
 * WINDOWED_TASKS=1. If the run hangs after 2 dots, try NUM_TASKS=2, WINDOWED_TASKS=0.
 * If it hangs after more dots (e.g. ".... (4/10 done)" then dumps), the fault may be
 * when switching to the next task; try increasing SPILL_STACK_WORDS or reducing
 * WINDOWED_TASKS to see if a particular window index or legacy switch is involved.
 * If suspension order is all the same (e.g. 0,0,0,...,0), the simulator is not
 * applying the staged register window from CSR 0x801 on mret; fix the Spike
 * mret path so the visible GPR bank is switched to the partition in 0x801.
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#if ( configUSE_RISCV_REGISTER_WINDOWS != 1 )
    #error Build with configUSE_RISCV_REGISTER_WINDOWS=1
#endif

#define UART0           ((volatile char *)0x10000000)
#define NUM_TASKS       10   /* more tasks than windows => spill: some get window, rest legacy */
#define YIELDS_PER_TASK 15   /* each task yields this many times then suspends */
#define SPILL_STACK_WORDS 256   /* generous per-task stack; reduce if RAM tight */
/* NXPR=256 => 8 windows (0=kernel, 1..7 for tasks). NXPR=64 => 1 task window only. */
#define WINDOWED_TASKS  7    /* 7 for NXPR=256 (tasks 0..6 windowed, 7..9 legacy); use 1 for NXPR=64 */

/* Set to 1 (or build with -DSPILL_DEBUG_TRACE=1) to print "[SPILL] id=N" each time a task runs; grep trace.log to see which id (a0) each run sees. */
#ifndef SPILL_DEBUG_TRACE
#define SPILL_DEBUG_TRACE 0
#endif

static volatile uint32_t counters[NUM_TASKS];
static volatile uint32_t done_count;
/* Order in which tasks suspended: suspended_id[n] = task id that suspended as (n+1)-th. */
static volatile uint32_t suspended_id[NUM_TASKS];

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

static void print_report( void )
{
    putstr( "\r\n--- All " );
    putdec( ( uint32_t ) NUM_TASKS );
    putstr( " tasks ran; graceful degradation OK ---\r\n" );
    putstr( "  Suspension order (id that suspended 1st, 2nd, ...): " );
    for ( int i = 0; i < NUM_TASKS; i++ )
    {
        putdec( suspended_id[ i ] );
        if ( i < NUM_TASKS - 1 ) putstr( "," );
    }
    putstr( "\r\n" );
    /* If all ids are the same, the simulator is not applying the register window from CSR 0x801 on mret. */
    {
        uint32_t first = suspended_id[ 0 ];
        uint32_t all_same = 1;
        for ( int i = 1; i < NUM_TASKS && all_same; i++ )
            if ( suspended_id[ i ] != first ) all_same = 0;
        if ( all_same && NUM_TASKS > 1 )
        {
            putstr( "  [CHECK] All suspended ids are the same => CSR 0x801 (window) not applied on mret in sim.\r\n" );
        }
    }
    for ( int i = 0; i < NUM_TASKS; i++ )
    {
        putstr( "  Task " ); putdec( ( uint32_t ) i );
        putstr( ": count=" ); putdec( counters[ i ] );
        putstr( "\r\n" );
    }
}

static void vTaskSpill( void * pv )
{
    uint32_t id = ( uint32_t ) ( uintptr_t ) pv;
    for ( ; ; )
    {
#if ( SPILL_DEBUG_TRACE == 1 )
        putstr( "[SPILL] id=" );
        putdec( id );
        putstr( "\r\n" );
#endif
        counters[ id ]++;
        if ( counters[ id ] >= YIELDS_PER_TASK )
        {
            uint32_t n = done_count;
            done_count++;
            suspended_id[ n ] = id;  /* record which id suspended as (n+1)-th (diagnostic for window restore) */
            *((volatile char *)0x10000000) = '.';  /* progress: one dot per task that finished */
            if ( done_count < NUM_TASKS )
            {
                putstr( " (" );
                putdec( done_count );
                putstr( "/" );
                putdec( ( uint32_t ) NUM_TASKS );
                putstr( " done)\r\n" );
            }
            vTaskSuspend( NULL );
            continue;
        }
        taskYIELD();
    }
}

static volatile uint8_t report_printed = 0;

void vApplicationIdleHook( void )
{
    /* Print report from idle (kernel/window 0) so we read counters[] in a consistent context.
     * If windowed tasks have wrong a0 on restore, counts can be wrong; suspension order helps diagnose. */
    if ( report_printed == 0 && done_count >= NUM_TASKS )
    {
        report_printed = 1;
        print_report();
    }
}

#define IDLE_STACK_WORDS  256   /* enough for report loop (putdec, putstr) */
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
    putstr( "\r\n=== Phase 4: Window Spilling (scalability) ===\r\n" );
    putdec( ( uint32_t ) NUM_TASKS );
    putstr( " tasks; first " );
    putdec( ( uint32_t ) WINDOWED_TASKS );
    putstr( " windowed, rest legacy. Each runs " );
    putdec( ( uint32_t ) YIELDS_PER_TASK );
    putstr( " yields then suspends (dots = tasks done).\r\n" );

    done_count = 0;
    report_printed = 0;
    for ( int i = 0; i < NUM_TASKS; i++ )
    {
        counters[ i ] = 0;
        suspended_id[ i ] = 0;  /* overwritten when each task suspends (order diagnostic) */
    }

    static StackType_t xStacks[ NUM_TASKS ][ SPILL_STACK_WORDS ];
    static StaticTask_t xTCBs[ NUM_TASKS ];
    TaskHandle_t handles[ NUM_TASKS ];

    for ( int i = 0; i < NUM_TASKS; i++ )
    {
        handles[ i ] = xTaskCreateStatic( vTaskSpill, "T", SPILL_STACK_WORDS,
                                          ( void * ) ( uintptr_t ) i, 1,
                                          xStacks[ i ], &xTCBs[ i ] );
        /* Assign window only to first WINDOWED_TASKS. NXPR=64 => only base 32; NXPR=256 => bases 32,64,...,224. */
        if ( handles[ i ] != NULL && i < WINDOWED_TASKS )
        {
            uint32_t base = 32 + ( uint32_t ) i * 32;
            vPortTaskSetRegisterWindow( handles[ i ], base, 32 );
        }
    }

    putstr( "Starting scheduler...\r\n" );
    vTaskStartScheduler();
    for ( ; ; ) { }
    return 0;
}
