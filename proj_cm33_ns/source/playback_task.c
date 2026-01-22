/******************************************************************************
* File Name: playback_task.c
*
* Description: WAV file playback task implementation
*              Receives PCM chunks from FileReadTask and streams to I2S
*
*******************************************************************************/

#include "playback_task.h"
#include "file_read_task.h"
#include "wav_file.h"
#include "app_i2s.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************************
* Global Variables
*******************************************************************************/
QueueHandle_t playback_queue = NULL;
TaskHandle_t playback_task_handle = NULL;

/* Playback state variables (shared with I2S ISR) */
volatile int16_t *playback_buffer_ptr = NULL;
volatile uint32_t playback_samples_remaining = 0;
volatile bool playback_active = false;

/*******************************************************************************
* Local Variables
*******************************************************************************/
/* Ping-pong buffers removed - using recorded_data buffer directly */

/*******************************************************************************
* Function Name: playback_task
********************************************************************************
* Summary:
*  Playback task - receives PCM chunks from FileReadTask and streams to I2S
*
* Parameters:
*  pvParameters: Task parameters (unused)
*
* Return:
*  None
*
*******************************************************************************/
void playback_task(void *pvParameters)
{
    (void)pvParameters;
    pcm_playback_msg_t pcm_msg;
    
    /* Add startup delay to prevent printf collision */
    vTaskDelay(pdMS_TO_TICKS(400));
    
    printf("=== Playback Task Started ===\r\n");
    
    while (1) {
        /* Wait for PCM chunk from FileReadTask */
        if (xQueueReceive(pcm_playback_queue, &pcm_msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        printf("[PlaybackTask] Received %u samples%s\r\n",
               (unsigned int)pcm_msg.sample_count,
               pcm_msg.is_last_chunk ? " (LAST)" : "");
        
        /* Set up I2S playback for this chunk */
        taskENTER_CRITICAL();
        playback_buffer_ptr = pcm_msg.buffer_ptr;
        playback_samples_remaining = pcm_msg.sample_count;
        playback_active = true;
        taskEXIT_CRITICAL();
        
        /* Wait for I2S ISR to finish consuming this chunk */
        while (playback_samples_remaining > 0 && playback_active) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        /* Check if this was the last chunk */
        if (pcm_msg.is_last_chunk) {
            taskENTER_CRITICAL();
            playback_active = false;
            playback_buffer_ptr = NULL;
            taskEXIT_CRITICAL();
            
            printf("[PlaybackTask] Playback complete\r\n");
        }
    }
}

/*******************************************************************************
* Function Name: playback_task_create
********************************************************************************
* Summary:
*  Create playback task (queue created in freertos_setup.c)
*
*******************************************************************************/
void playback_task_create(void)
{
    /* Create playback task */
    if (xTaskCreate(playback_task,
                    "Playback",
                    PLAYBACK_TASK_STACK_SIZE,
                    NULL,
                    PLAYBACK_TASK_PRIORITY,
                    &playback_task_handle) != pdPASS) {
        printf("Error: Failed to create playback task\r\n");
    }
}
