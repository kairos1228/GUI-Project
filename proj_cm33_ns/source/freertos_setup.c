/******************************************************************************
* File Name: freertos_setup.c
*
* Description: FreeRTOS system initialization implementation
*
*******************************************************************************/

#include "freertos_setup.h"
#include "cli_task.h"
// #include "playback_task.h"  /* Temporarily disabled */
#include "app_pdm_pcm.h"
#include "app_i2s.h"
#include <stdio.h>

/*******************************************************************************
* Global Variables - IPC Objects
*******************************************************************************/
EventGroupHandle_t audio_state_events = NULL;
SemaphoreHandle_t buffer_free_sem = NULL;

/*******************************************************************************
* Function Name: freertos_create_ipc_objects
********************************************************************************
* Summary:
*  Create all FreeRTOS IPC objects (queues, semaphores, event groups)
*
* Parameters:
*  None
*
* Return:
*  0 on success, -1 on error
*
*******************************************************************************/
static int freertos_create_ipc_objects(void)
{
    /* Create audio state event group for broadcasting system state */
    audio_state_events = xEventGroupCreate();
    if (audio_state_events == NULL) {
        printf("Error: Failed to create audio state event group\r\n");
        return -1;
    }
    
    /* Set initial state to IDLE */
    xEventGroupSetBits(audio_state_events, EVENT_IDLE);
    
    /* Create buffer free semaphore for ping-pong buffer synchronization */
    buffer_free_sem = xSemaphoreCreateCounting(2, 2);  /* Max 2, initial 2 */
    if (buffer_free_sem == NULL) {
        printf("Error: Failed to create buffer free semaphore\r\n");
        return -1;
    }
    
    printf("IPC objects created successfully\r\n");
    return 0;
}

/*******************************************************************************
* Function Name: freertos_create_tasks
********************************************************************************
* Summary:
*  Create all application tasks
*
* Parameters:
*  None
*
* Return:
*  0 on success, -1 on error
*
*******************************************************************************/
static int freertos_create_tasks(void)
{
    /* Create CLI Task (handles UART commands) */
    cli_task_create();
    if (cli_task_handle == NULL) {
        printf("Error: CLI task creation failed\r\n");
        return -1;
    }
    printf("CLI Task created\r\n");
    
    /* Create Playback Task (handles WAV file playback) */
    /* Temporarily disabled - requires SD card driver */
    /*
    playback_task_create();
    if (playback_task_handle == NULL) {
        printf("Error: Playback task creation failed\r\n");
        return -1;
    }
    printf("Playback Task created\r\n");
    */
    
    /* Note: Audio Control Task and File Write Task will be created here */
    /* when those modules are implemented */
    
    return 0;
}

/*******************************************************************************
* Function Name: freertos_system_init
********************************************************************************
* Summary:
*  Initialize entire FreeRTOS system
*  - Create IPC objects (queues, semaphores, event groups)
*  - Create all tasks
*  - Start scheduler
*
* Parameters:
*  None
*
* Return:
*  This function does not return (scheduler takes over)
*
*******************************************************************************/
void freertos_system_init(void)
{
    printf("\r\n=== FreeRTOS System Initialization ===\r\n");
    
    /* Step 1: Create IPC objects */
    if (freertos_create_ipc_objects() != 0) {
        printf("FATAL: IPC object creation failed\r\n");
        while(1);  /* Halt system */
    }
    
    /* Step 2: Initialize hardware drivers (PDM, I2S, Codec) */
    printf("Initializing audio hardware...\r\n");
    app_tlv_codec_init();
    app_i2s_init();
    app_pdm_pcm_init();
    printf("Audio hardware initialized\r\n");
    
    /* Step 3: Create all application tasks */
    if (freertos_create_tasks() != 0) {
        printf("FATAL: Task creation failed\r\n");
        while(1);  /* Halt system */
    }
    
    printf("=== Starting FreeRTOS Scheduler ===\r\n\r\n");
    
    /* Step 4: Start FreeRTOS scheduler (does not return) */
    vTaskStartScheduler();
    
    /* Should never reach here */
    printf("FATAL: Scheduler failed to start\r\n");
    while(1);
}

/*******************************************************************************
* FreeRTOS Hook Functions
*******************************************************************************/

/**
 * @brief FreeRTOS Idle Hook
 * Called when no tasks are ready to run
 */
void vApplicationIdleHook(void)
{
    /* Enter low power mode or perform background tasks */
    __WFI();  /* Wait For Interrupt */
}

/**
 * @brief FreeRTOS Stack Overflow Hook
 * Called when stack overflow is detected (if configCHECK_FOR_STACK_OVERFLOW > 0)
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("FATAL: Stack overflow in task: %s\r\n", pcTaskName);
    __disable_irq();
    while(1);
}

/**
 * @brief FreeRTOS Malloc Failed Hook
 * Called when pvPortMalloc fails
 */
void vApplicationMallocFailedHook(void)
{
    printf("FATAL: Heap allocation failed\r\n");
    __disable_irq();
    while(1);
}

/**
 * @brief FreeRTOS Tick Hook
 * Called on each tick interrupt (if configUSE_TICK_HOOK == 1)
 */
void vApplicationTickHook(void)
{
    /* Optional: Add periodic tasks here */
}
