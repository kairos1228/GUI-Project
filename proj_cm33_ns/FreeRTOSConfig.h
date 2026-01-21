/*
 * FreeRTOS Kernel V10.4.3
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *-----------------------------------------------------------*/

/* Cortex-M33 specific definitions. */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS         __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS         3
#endif

/* The lowest interrupt priority that can be used in a call to a "set priority" function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         0x07

/* The highest interrupt priority that can be used by any interrupt service
 * routine that makes calls to interrupt safe FreeRTOS API functions. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

/* Interrupt priorities used by the kernel port layer itself. */
#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

/* System clock frequency (100MHz for PSoC Edge) */
#define configCPU_CLOCK_HZ                              ( ( unsigned long ) 100000000 )
#define configTICK_RATE_HZ                              ( ( TickType_t ) 1000 )

/* Scheduler configuration */
#define configUSE_PREEMPTION                            1
#define configUSE_TIME_SLICING                          1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION         0
#define configUSE_TICKLESS_IDLE                         0
#define configMAX_PRIORITIES                            ( 7 )
#define configMINIMAL_STACK_SIZE                        ( ( unsigned short ) 128 )
#define configMAX_TASK_NAME_LEN                         ( 16 )
#define configUSE_16_BIT_TICKS                          0
#define configIDLE_SHOULD_YIELD                         1

/* Memory allocation */
#define HEAP_ALLOCATION_TYPE1                           (1)     /* heap_1.c */
#define HEAP_ALLOCATION_TYPE2                           (2)     /* heap_2.c */
#define HEAP_ALLOCATION_TYPE3                           (3)     /* heap_3.c */
#define HEAP_ALLOCATION_TYPE4                           (4)     /* heap_4.c */
#define HEAP_ALLOCATION_TYPE5                           (5)     /* heap_5.c */

#define configSUPPORT_STATIC_ALLOCATION                 1
#define configSUPPORT_DYNAMIC_ALLOCATION                1
#define configTOTAL_HEAP_SIZE                           ( ( size_t ) ( 65536 ) )  /* 64KB */
#define configAPPLICATION_ALLOCATED_HEAP                0
#define configHEAP_ALLOCATION_SCHEME                    HEAP_ALLOCATION_TYPE4

/* Hook function configuration */
#define configUSE_IDLE_HOOK                             1
#define configUSE_TICK_HOOK                             1
#define configCHECK_FOR_STACK_OVERFLOW                  2
#define configUSE_MALLOC_FAILED_HOOK                    1
#define configUSE_DAEMON_TASK_STARTUP_HOOK              0

/* Run time and task stats gathering */
#define configGENERATE_RUN_TIME_STATS                   0
#define configUSE_TRACE_FACILITY                        1
#define configUSE_STATS_FORMATTING_FUNCTIONS            0

/* Co-routine definitions */
#define configUSE_CO_ROUTINES                           0
#define configMAX_CO_ROUTINE_PRIORITIES                 ( 2 )

/* Software timer definitions */
#define configUSE_TIMERS                                1
#define configTIMER_TASK_PRIORITY                       ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                        10
#define configTIMER_TASK_STACK_DEPTH                    ( configMINIMAL_STACK_SIZE * 2 )

/* Set the following definitions to 1 to include the API function */
#define INCLUDE_vTaskPrioritySet                        1
#define INCLUDE_uxTaskPriorityGet                       1
#define INCLUDE_vTaskDelete                             1
#define INCLUDE_vTaskCleanUpResources                   0
#define INCLUDE_vTaskSuspend                            1
#define INCLUDE_vTaskDelayUntil                         1
#define INCLUDE_vTaskDelay                              1
#define INCLUDE_xTaskGetSchedulerState                  1
#define INCLUDE_xTimerPendFunctionCall                  1
#define INCLUDE_xQueueGetMutexHolder                    1
#define INCLUDE_uxTaskGetStackHighWaterMark             1
#define INCLUDE_eTaskGetState                           1

/* Additional API features */
#define configUSE_MUTEXES                               1
#define configUSE_RECURSIVE_MUTEXES                     1
#define configUSE_COUNTING_SEMAPHORES                   1
#define configUSE_QUEUE_SETS                            0
#define configUSE_TASK_NOTIFICATIONS                    1

/* Cortex-M specific definitions */
#define configENABLE_MPU                                0
#define configENABLE_FPU                                1
#define configENABLE_TRUSTZONE                          0

/* Assert configuration */
#define configASSERT( x )    if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* Definitions that map the FreeRTOS port interrupt handlers to their CMSIS standard names */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
