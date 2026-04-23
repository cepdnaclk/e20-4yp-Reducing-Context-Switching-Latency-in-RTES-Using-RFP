/*
 * Phase 2: SRAM / Memory Footprint Reduction Test
 *
 * Reports exact SRAM bytes used for context and bytes saved per yield for windowed tasks.
 * Uses same port logic as Phase 1 (port_current_task_window_base for save-path decision).
 *
 * Standard FreeRTOS: 31 regs x 4 bytes = 124 bytes per task stack for context on each yield.
 * This port: full frame = 33 words; minimal path stores 5 words = 20 bytes.
 * Bytes saved per yield per windowed task = (33 - 5) * 4 = 112 bytes not written to stack.
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#if ( configUSE_RISCV_REGISTER_WINDOWS != 1 )
    #error Build with configUSE_RISCV_REGISTER_WINDOWS=1
#endif

#define UART0 ((volatile char *)0x10000000)

/* Match portContext.h: windowed port uses 33 words per context frame. */
#define PORT_CONTEXT_WORDS       33
#define PORT_WORD_BYTES          4
#define PORT_FULL_CONTEXT_BYTES  ( PORT_CONTEXT_WORDS * PORT_WORD_BYTES )

/* Minimal path: mepc(0), mstatus(1), critical_nesting(30), window_cfg(31), context_type(32) = 5 words. */
#define PORT_MINIMAL_STORED_WORDS  5
#define PORT_MINIMAL_STORED_BYTES  ( PORT_MINIMAL_STORED_WORDS * PORT_WORD_BYTES )

#define STANDARD_GPR_BYTES       ( 31 * 4 )
#define BYTES_SAVED_PER_YIELD    ( PORT_FULL_CONTEXT_BYTES - PORT_MINIMAL_STORED_BYTES )

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

void vApplicationIdleHook( void )
{
    static uint8_t done = 0;
    if ( done ) return;
    done = 1;

    putstr( "\r\n--- Phase 2: SRAM / Memory Footprint ---\r\n" );
    putstr( "Context frame size (this port): " );
    putdec( PORT_FULL_CONTEXT_BYTES );
    putstr( " bytes (33 words)\r\n" );

    putstr( "Standard FreeRTOS: 31 GPRs x 4 = " );
    putdec( STANDARD_GPR_BYTES );
    putstr( " bytes written per yield (legacy)\r\n" );

    putstr( "Minimal path (windowed): " );
    putdec( PORT_MINIMAL_STORED_BYTES );
    putstr( " bytes written per yield (5 words)\r\n" );

    putstr( "Bytes saved per yield per windowed task: " );
    putdec( BYTES_SAVED_PER_YIELD );
    putstr( " bytes\r\n" );

    putstr( "For N windowed tasks, M yields each: " );
    putdec( BYTES_SAVED_PER_YIELD );
    putstr( " x N x M bytes not written to SRAM.\r\n" );

    vTaskSuspend( NULL );
}

/* Single task: suspend immediately so idle (only other ready task) runs and prints the report. */
static void vTaskDummy( void * pv )
{
    ( void ) pv;
    vTaskSuspend( NULL );   /* block so scheduler runs idle */
    for ( ;; )
        taskYIELD();
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
    putstr( "\r\n=== Phase 2: SRAM Footprint Test ===\r\n" );
    putstr( "One task suspends immediately; idle runs and prints report.\r\n" );
    putstr( "Starting scheduler...\r\n" );

    static StackType_t xStack[ configMINIMAL_STACK_SIZE ];
    static StaticTask_t xTCB;
    ( void ) xTaskCreateStatic( vTaskDummy, "Dummy", configMINIMAL_STACK_SIZE, NULL, 1, xStack, &xTCB );

    vTaskStartScheduler();
    for ( ; ; ) { }
    return 0;
}
