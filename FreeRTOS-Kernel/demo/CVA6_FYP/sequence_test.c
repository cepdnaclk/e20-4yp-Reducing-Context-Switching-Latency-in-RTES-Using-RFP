#include "common.h"

#define TASKS_COUNT       4U
#define SEQ_ITERATIONS    12U
#define LOG_SIZE          ( TASKS_COUNT * SEQ_ITERATIONS )

static volatile uint32_t ulDoneCount = 0U;
static volatile uint32_t ulLogIndex = 0U;
static volatile uint8_t ucSequenceLog[ LOG_SIZE ];

static void prvSeqTask( void * pvArg )
{
    const uint32_t ulTaskId = ( uint32_t ) ( uintptr_t ) pvArg;
    uint32_t i;

    for( i = 0; i < SEQ_ITERATIONS; i++ )
    {
        if( ulLogIndex < LOG_SIZE )
        {
            ucSequenceLog[ ulLogIndex ] = ( uint8_t ) ulTaskId;
            ulLogIndex++;
        }

        taskYIELD();
    }

    ulDoneCount++;
    vTaskSuspend( NULL );
}

static BaseType_t prvCheckRoundRobinSequence( void )
{
    uint32_t i;

    for( i = 0; i < LOG_SIZE; i++ )
    {
        if( ucSequenceLog[ i ] != ( uint8_t ) ( i % TASKS_COUNT ) )
        {
            return pdFALSE;
        }
    }

    return pdTRUE;
}

int main( void )
{
    static StaticTask_t xTCB[ TASKS_COUNT ];
    static StackType_t xStack[ TASKS_COUNT ][ configMINIMAL_STACK_SIZE ];
    TaskHandle_t xTask;
    uint32_t i;

    cv6_putstr( "\r\n[PHASE3] sequence_test start\r\n" );

    for( i = 0; i < TASKS_COUNT; i++ )
    {
        xTask = xTaskCreateStatic( prvSeqTask, "SEQ", configMINIMAL_STACK_SIZE, ( void * ) ( uintptr_t ) i, 1, xStack[ i ], &xTCB[ i ] );

        if( xTask == NULL )
        {
            cv6_finish( pdFALSE, 0xE311U, i, 0U );
        }

        /* Variable window widths preserve requested strategy coverage. */
        vPortTaskSetRegisterWindow( xTask, 32U + ( i * 8U ), 8U + ( i * 4U ) );
    }

    vTaskStartScheduler();
    cv6_finish( pdFALSE, 0xE312U, 0U, 0U );
    return 0;
}

void vApplicationIdleHook( void )
{
    BaseType_t xPass = pdFALSE;

    if( ulDoneCount < TASKS_COUNT )
    {
        return;
    }

    xPass = prvCheckRoundRobinSequence();

    if( xPass != pdFALSE )
    {
        cv6_putstr( "[PHASE3] sequence_test PASS\r\n" );
    }
    else
    {
        cv6_putstr( "[PHASE3] sequence_test FAIL idx=" );
        cv6_puthex32( ulLogIndex );
        cv6_putstr( "\r\n" );
    }

    cv6_finish( xPass, 0xA311U, ulLogIndex, ucSequenceLog[ LOG_SIZE - 1U ] );
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
