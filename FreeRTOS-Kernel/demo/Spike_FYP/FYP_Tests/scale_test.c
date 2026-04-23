/*
 * FYP Scale Test: 4 tasks round-robin (3 legacy + 1 windowed).
 * Demonstrates scheduler scaling with one task using register window (base 32).
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#if ( configUSE_RISCV_REGISTER_WINDOWS != 1 )
    #error Build with configUSE_RISCV_REGISTER_WINDOWS=1
#endif

#define UART0  ((volatile char *)0x10000000)

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

static volatile uint32_t counters[4];

void vApplicationIdleHook( void )
{
    /* No-op for scale test. */
}

static void vTaskScale( void * pv )
{
    uint32_t id = ( uint32_t )( uintptr_t ) pv;
    for ( ; ; )
    {
        counters[id]++;
        putstr( "  [T" );
        putdec( id );
        putstr( "] " );
        putdec( counters[id] );
        putstr( "\r\n" );
        taskYIELD();
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
    putstr( "\r\n=== FYP Scale Test (4 tasks: T0,T1,T2 legacy, T3 windowed) ===\r\n" );

    static StackType_t xStack0[ configMINIMAL_STACK_SIZE ];
    static StackType_t xStack1[ configMINIMAL_STACK_SIZE ];
    static StackType_t xStack2[ configMINIMAL_STACK_SIZE ];
    static StackType_t xStack3[ configMINIMAL_STACK_SIZE ];
    static StaticTask_t xTCB0, xTCB1, xTCB2, xTCB3;
    TaskHandle_t x3 = NULL;

    ( void ) xTaskCreateStatic( vTaskScale, "T0", configMINIMAL_STACK_SIZE, ( void * ) 0, 1, xStack0, &xTCB0 );
    ( void ) xTaskCreateStatic( vTaskScale, "T1", configMINIMAL_STACK_SIZE, ( void * ) 1, 1, xStack1, &xTCB1 );
    ( void ) xTaskCreateStatic( vTaskScale, "T2", configMINIMAL_STACK_SIZE, ( void * ) 2, 1, xStack2, &xTCB2 );
    x3 = xTaskCreateStatic( vTaskScale, "T3", configMINIMAL_STACK_SIZE, ( void * ) 3, 1, xStack3, &xTCB3 );
    if ( x3 != NULL )
        vPortTaskSetRegisterWindow( x3, 32, 32 );

    vTaskStartScheduler();
    for ( ; ; ) { }
    return 0;
}
