#include "common.h"

#define ITERATIONS    48U

static volatile uint32_t ulTaskDone = 0U;
static volatile uint32_t ulTaskErrors = 0U;

static void prvIntegrityTask( void * pvArg )
{
    const uint32_t ulSentinel = ( uint32_t ) ( uintptr_t ) pvArg;
    register uint32_t regSentinel __asm__( "s0" );
    uint32_t i;

    regSentinel = ulSentinel;

    for( i = 0; i < ITERATIONS; i++ )
    {
        if( regSentinel != ulSentinel )
        {
            ulTaskErrors++;
        }

        taskYIELD();
    }

    ulTaskDone++;
    vTaskSuspend( NULL );
}

int main( void )
{
    static StaticTask_t xTCB0, xTCB1, xTCB2;
    static StackType_t xStack0[ configMINIMAL_STACK_SIZE ];
    static StackType_t xStack1[ configMINIMAL_STACK_SIZE ];
    static StackType_t xStack2[ configMINIMAL_STACK_SIZE ];
    TaskHandle_t xTask0;
    TaskHandle_t xTask1;
    TaskHandle_t xTask2;

    cv6_putstr( "\r\n[PHASE3] integrity_test start\r\n" );

    xTask0 = xTaskCreateStatic( prvIntegrityTask, "W0", configMINIMAL_STACK_SIZE, ( void * ) 0xA0A0A0A0UL, 1, xStack0, &xTCB0 );
    xTask1 = xTaskCreateStatic( prvIntegrityTask, "W1", configMINIMAL_STACK_SIZE, ( void * ) 0xB1B1B1B1UL, 1, xStack1, &xTCB1 );
    xTask2 = xTaskCreateStatic( prvIntegrityTask, "KB", configMINIMAL_STACK_SIZE, ( void * ) 0xC2C2C2C2UL, 1, xStack2, &xTCB2 );

    if( ( xTask0 == NULL ) || ( xTask1 == NULL ) || ( xTask2 == NULL ) )
    {
        cv6_finish( pdFALSE, 0xE301U, 0U, 0U );
    }

    vPortTaskSetRegisterWindow( xTask0, 32U, 16U );
    vPortTaskSetRegisterWindow( xTask1, 48U, 16U );
    vPortTaskSetRegisterWindow( xTask2, 0U, 32U );
    vPortTaskSetKernelBorrowing( xTask2, pdTRUE );

    vTaskStartScheduler();
    cv6_finish( pdFALSE, 0xE302U, 0U, 0U );
    return 0;
}

void vApplicationIdleHook( void )
{
    uint32_t ulInvalidMinimal = 0U;
    uint32_t ulBorrowRestores = 0U;
    uint32_t ulKernelStages = 0U;
    BaseType_t xPass = pdFALSE;

    if( ulTaskDone < 3U )
    {
        return;
    }

    vPortGetWindowIntegrityCounters( &ulInvalidMinimal, &ulBorrowRestores, &ulKernelStages );

    xPass = ( ulTaskErrors == 0U ) && ( ulInvalidMinimal == 0U );

    if( xPass != pdFALSE )
    {
        cv6_putstr( "[PHASE3] integrity_test PASS\r\n" );
    }
    else
    {
        cv6_putstr( "[PHASE3] integrity_test FAIL err=" );
        cv6_puthex32( ulTaskErrors );
        cv6_putstr( " inv=" );
        cv6_puthex32( ulInvalidMinimal );
        cv6_putstr( "\r\n" );
    }

    cv6_finish( xPass, 0xA301U, ulTaskErrors, ulKernelStages + ( ulBorrowRestores << 16 ) );
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
