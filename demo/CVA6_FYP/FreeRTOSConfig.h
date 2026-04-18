/*
 * FreeRTOSConfig.h for CVA6 + register-window kernel tests.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_PREEMPTION                       1
#define configUSE_TICKLESS_IDLE                    0
#define configUSE_IDLE_HOOK                        1
#define configUSE_TICK_HOOK                        0
#define configCPU_CLOCK_HZ                         ( ( unsigned long ) 10000000 )
#define configTICK_RATE_HZ                         ( ( TickType_t ) 100 )
#define configMAX_PRIORITIES                       ( 5 )
#define configMINIMAL_STACK_SIZE                   ( ( uint16_t ) 192 )
#define configMAX_TASK_NAME_LEN                    ( 16 )
#define configTICK_TYPE_WIDTH_IN_BITS              TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                    1
#define configUSE_MUTEXES                          0
#define configUSE_RECURSIVE_MUTEXES                0
#define configUSE_COUNTING_SEMAPHORES              0
#define configUSE_ALTERNATIVE_API                  0
#define configQUEUE_REGISTRY_SIZE                  0
#define configUSE_QUEUE_SETS                       0
#define configUSE_TIME_SLICING                     1
#define configUSE_NEWLIB_REENTRANT                 0
#define configENABLE_BACKWARD_COMPATIBILITY        0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS    0
#define configUSE_MINI_LIST_ITEM                   1
#define configSTACK_DEPTH_TYPE                     size_t

#define configUSE_TIMERS                           0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    0
#define configUSE_TRACE_FACILITY                   0
#define configUSE_STATS_FORMATTING_FUNCTIONS       0
#define configGENERATE_RUN_TIME_STATS              0

#define configSUPPORT_STATIC_ALLOCATION            1
#define configSUPPORT_DYNAMIC_ALLOCATION           0
#define configAPPLICATION_ALLOCATED_HEAP           0

/* CVA6 tests use voluntary taskYIELD() context switches by default. */
#define configMTIME_BASE_ADDRESS                   ( 0UL )
#define configMTIMECMP_BASE_ADDRESS                ( 0UL )

#define configUSE_RISCV_REGISTER_WINDOWS           1
#define configRISCV_WINDOWED_KERNEL_MODE           1
#define configRISCV_WINDOW_BORROW_GUARD            1

#define INCLUDE_vTaskSuspend                       1

#define configASSERT( x )                                      \
    do                                                         \
    {                                                          \
        if( ( x ) == 0 )                                       \
        {                                                      \
            (*(volatile unsigned char *)0x10000000) = '!';     \
            taskDISABLE_INTERRUPTS();                          \
            for( ;; )                                          \
            {                                                  \
            }                                                  \
        }                                                      \
    } while( 0 )
#define configASSERT_DEFINED 1

#endif /* FREERTOS_CONFIG_H */
