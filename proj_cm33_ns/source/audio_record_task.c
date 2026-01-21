/******************************************************************************
* File Name: audio_record_task.c
*
* Description: Audio recording task implementation
*              - Monitors EVENT_RECORDING flag
*              - Activates/deactivates PDM hardware
*              - Detects buffer overflow
*              - Sends completed buffers to FileWriteTask
*
*******************************************************************************/

#include "audio_record_task.h"
#include "freertos_setup.h"
#include "app_pdm_pcm.h"
#include "wav_file.h"
#include <stdio.h>

/*******************************************************************************
* Global Variables
*******************************************************************************/
TaskHandle_t audio_record_task_handle = NULL;
/* audio_record_queue is defined in freertos_setup.c */

/*******************************************************************************
* Local Variables
*******************************************************************************/
static uint32_t last_sample_count = 0;

/*******************************************************************************
* Function Name: audio_record_task
********************************************************************************
* Summary:
*  Audio recording management task
*  - Waits for EVENT_RECORDING flag from AudioControlTask
*  - Activates PDM hardware
*  - Monitors buffer fill status
*  - Sends completed recording to FileWriteTask via queue
*
* Parameters:
*  arg: Unused task parameter
*
*******************************************************************************/
static void audio_record_task(void *arg)
{
    (void)arg;
    EventBits_t event_bits;
    uint32_t current_sample_count;
    audio_record_msg_t record_msg;
    
    /* Small delay to avoid printf collision with other tasks */
    vTaskDelay(pdMS_TO_TICKS(100));
    
    printf("\r\n=== Audio Record Task Started ===\r\n");
    
    for (;;)
    {
        /* Wait for recording start event (block indefinitely) */
        event_bits = xEventGroupWaitBits(
            audio_state_events,
            EVENT_RECORDING,
            pdFALSE,  /* Don't clear on exit */
            pdFALSE,  /* Wait for any bit */
            portMAX_DELAY
        );
        
        if (event_bits & EVENT_RECORDING)
        {
            printf("[RecordTask] Recording event detected, activating PDM...\r\n");
            
            /* Initialize tracking */
            last_sample_count = 0;
            
            /* Activate PDM hardware (audio_data_ptr is reset inside) */
            app_pdm_pcm_activate();
            
            /* Monitor recording progress */
            while (1)
            {
                /* Check if recording should stop */
                event_bits = xEventGroupGetBits(audio_state_events);
                
                if (!(event_bits & EVENT_RECORDING))
                {
                    /* Recording stopped by AudioControlTask */
                    printf("[RecordTask] Stop requested, deactivating PDM...\r\n");
                    app_pdm_pcm_deactivate();
                    
                    /* Get final sample count */
                    current_sample_count = get_audio_data_index();
                    
                    printf("[RecordTask] Recording complete: %lu samples\r\n", 
                           current_sample_count);
                    
                    /* Prepare message for FileWriteTask */
                    record_msg.buffer_ptr = (int16_t *)get_recorded_data_buffer();
                    record_msg.sample_count = current_sample_count;
                    record_msg.sample_rate = SAMPLE_RATE_HZ;
                    record_msg.num_channels = NUM_CHANNELS;
                    
                    /* Send to FileWriteTask (non-blocking, 100ms timeout) */
                    if (xQueueSend(audio_record_queue, &record_msg, pdMS_TO_TICKS(100)) != pdPASS)
                    {
                        printf("[RecordTask] ERROR: Failed to send to FileWriteTask queue\r\n");
                    }
                    else
                    {
                        printf("[RecordTask] Sent %lu samples to FileWriteTask\r\n", 
                               current_sample_count);
                    }
                    
                    /* Set recording done event */
                    xEventGroupSetBits(audio_state_events, EVENT_RECORDING_DONE);
                    
                    break;  /* Exit monitoring loop */
                }
                
                /* Check buffer overflow (full buffer condition) */
                current_sample_count = get_audio_data_index();
                
                if (current_sample_count >= (NUM_CHANNELS * BUFFER_SIZE))
                {
                    printf("[RecordTask] WARNING: Buffer full (%lu samples), stopping...\r\n",
                           current_sample_count);
                    
                    /* Auto-stop recording */
                    app_pdm_pcm_deactivate();
                    
                    /* Clear recording flag */
                    xEventGroupClearBits(audio_state_events, EVENT_RECORDING);
                    
                    /* Prepare and send message */
                    record_msg.buffer_ptr = (int16_t *)get_recorded_data_buffer();
                    record_msg.sample_count = current_sample_count;
                    record_msg.sample_rate = SAMPLE_RATE_HZ;
                    record_msg.num_channels = NUM_CHANNELS;
                    
                    if (xQueueSend(audio_record_queue, &record_msg, pdMS_TO_TICKS(100)) != pdPASS)
                    {
                        printf("[RecordTask] ERROR: Failed to send full buffer\r\n");
                    }
                    
                    xEventGroupSetBits(audio_state_events, EVENT_RECORDING_DONE);
                    
                    break;
                }
                
                /* Sleep for 100ms before next check */
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }
}

/*******************************************************************************
* Function Name: audio_record_task_create
********************************************************************************
* Summary:
*  Create the audio recording task
*
*******************************************************************************/
void audio_record_task_create(void)
{
    BaseType_t result = xTaskCreate(
        audio_record_task,
        "AudioRecord",
        AUDIO_RECORD_TASK_STACK_SIZE,
        NULL,
        AUDIO_RECORD_TASK_PRIORITY,
        &audio_record_task_handle
    );
    
    if (result != pdPASS)
    {
        printf("Error: Audio Record Task creation failed\r\n");
        audio_record_task_handle = NULL;
    }
}
