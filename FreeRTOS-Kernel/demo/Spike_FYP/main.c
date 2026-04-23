/*
 * FreeRTOS Round-Robin demo for Spike+FYP Register Windows.
 * Task A: legacy (full save/restore). Task B: register window (partition switch only).
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#if ( configUSE_RISCV_REGISTER_WINDOWS != 1 )
    #error Build with configUSE_RISCV_REGISTER_WINDOWS=1 and Spike chip extensions.
#endif

#define UART0  ((volatile char *)0x10000000)

static void putstr( const char * s )
{
    while ( *s ) { *UART0 = *s++; }
}

static void puthex( uint32_t x )
{
    const char h[] = "0123456789ABCDEF";
    putstr( "0x" );
    for ( int i = 7; i >= 0; i-- ) *UART0 = h[ ( x >> ( i * 4 ) ) & 0xF ];
}

/* Task A: no register window (legacy context switch). */
static void vTaskA( void * pv )
{
    ( void ) pv;
    uint32_t n = 0;
    for ( ; ; )
    {
        putstr( "\r\n[TASK A] legacy " );
        puthex( n++ );
        putstr( " -> yield\r\n" );
        taskYIELD();
    }
}

/* Task B: uses register window (base 32, size 32); zero-overhead switch. */
static void vTaskB( void * pv )
{
    ( void ) pv;
    uint32_t n = 0;
    for ( ; ; )
    {
        putstr( "    [TASK B] window " );
        puthex( n++ );
        putstr( " -> yield\r\n" );
        taskYIELD();
    }
}

int main( void )
{
    putstr( "\r\n=== FreeRTOS Spike+FYP Register Windows Demo ===\r\n" );

    static StackType_t xStackA[ configMINIMAL_STACK_SIZE ];
    static StackType_t xStackB[ configMINIMAL_STACK_SIZE ];
    static StaticTask_t xTCBA, xTCBB;
    TaskHandle_t xB = NULL;

    ( void ) xTaskCreateStatic( vTaskA, "A", configMINIMAL_STACK_SIZE, NULL, 1, xStackA, &xTCBA );
    xB = xTaskCreateStatic( vTaskB, "B", configMINIMAL_STACK_SIZE, NULL, 1, xStackB, &xTCBB );

    if ( xB != NULL )
        vPortTaskSetRegisterWindow( xB, 32, 32 );  /* base 32, size 32 = window 1 */

    putstr( "Starting scheduler (Task A = legacy, Task B = windowed).\r\n" );
    vTaskStartScheduler();
    for ( ; ; ) {}
    return 0;
}

/* Idle hook: if you see "[IDLE]" then scheduler is running idle instead of A/B. */
void vApplicationIdleHook( void )
{
    putstr( "[IDLE]" );
}

/* Required when configSUPPORT_STATIC_ALLOCATION is 1. */
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
