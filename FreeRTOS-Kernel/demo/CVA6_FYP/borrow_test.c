#include "common.h"

#define BORROW_TASKS     2U
#define WINDOW_TASKS     2U
#define TOTAL_TASKS      ( BORROW_TASKS + WINDOW_TASKS )
#define BORROW_LOOPS     24U

static volatile uint32_t ulTaskHits[ TOTAL_TASKS ];
static volatile uint32_t ulDone = 0U;
static volatile uint32_t ulSharedBorrowCounter = 0U;

static void prvBorrowTask( void * pvArg )
{
    const uint32_t ulId = ( uint32_t ) ( uintptr_t ) pvArg;
    uint32_t i;

    for( i = 0; i < BORROW_LOOPS; i++ )
    {
        ulTaskHits[ ulId ]++;
        ulSharedBorrowCounter++;
        taskYIELD();
    }

    ulDone++;
    vTaskSuspend( NULL );
}

static BaseType_t prvValidate( uint32_t ulKernelBorrowRestores,
                               uint32_t ulKernelStages )
{
    uint32_t i;

    for( i = 0; i < TOTAL_TASKS; i++ )
    {
        if( ulTaskHits[ i ] != BORROW_LOOPS )
        {
            return pdFALSE;
        }
    }

    if( ulSharedBorrowCounter != ( BORROW_LOOPS * BORROW_TASKS ) )
    {
        return pdFALSE;
    }

    if( ( ulKernelBorrowRestores == 0U ) || ( ulKernelStages == 0U ) )
    {
        return pdFALSE;
    }

    return pdTRUE;
}

int main( void )
{
    static StaticTask_t xTCB[ TOTAL_TASKS ];
    static StackType_t xStack[ TOTAL_TASKS ][ configMINIMAL_STACK_SIZE ];
    TaskHandle_t xTask;
    uint32_t i;

    cv6_putstr( "\r\n[PHASE3] borrow_test start\r\n" );

    for( i = 0; i < TOTAL_TASKS; i++ )
    {
        xTask = xTaskCreateStatic( prvBorrowTask, "BRW", configMINIMAL_STACK_SIZE, ( void * ) ( uintptr_t ) i, 1, xStack[ i ], &xTCB[ i ] );

        if( xTask == NULL )
        {
            cv6_finish( pdFALSE, 0xE321U, i, 0U );
        }

        if( i < BORROW_TASKS )
        {
            vPortTaskSetRegisterWindow( xTask, 0U, 32U );
            vPortTaskSetKernelBorrowing( xTask, pdTRUE );
        }
        else
        {
            vPortTaskSetRegisterWindow( xTask, 32U + ( ( i - BORROW_TASKS ) * 16U ), 16U );
            vPortTaskSetKernelBorrowing( xTask, pdFALSE );
        }
    }

    vTaskStartScheduler();
    cv6_finish( pdFALSE, 0xE322U, 0U, 0U );
    return 0;
}

void vApplicationIdleHook( void )
{
    uint32_t ulInvalidMinimal = 0U;
    uint32_t ulKernelBorrowRestores = 0U;
    uint32_t ulKernelStages = 0U;
    BaseType_t xPass = pdFALSE;

    if( ulDone < TOTAL_TASKS )
    {
        return;
    }

    vPortGetWindowIntegrityCounters( &ulInvalidMinimal, &ulKernelBorrowRestores, &ulKernelStages );
    xPass = ( ulInvalidMinimal == 0U ) && prvValidate( ulKernelBorrowRestores, ulKernelStages );

    if( xPass != pdFALSE )
    {
        cv6_putstr( "[PHASE3] borrow_test PASS\r\n" );
    }
    else
    {
        cv6_putstr( "[PHASE3] borrow_test FAIL inv=" );
        cv6_puthex32( ulInvalidMinimal );
        cv6_putstr( " kb=" );
        cv6_puthex32( ulKernelBorrowRestores );
        cv6_putstr( "\r\n" );
    }

    cv6_finish( xPass, 0xA321U, ulInvalidMinimal, ulKernelBorrowRestores + ( ulKernelStages << 16 ) );
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
