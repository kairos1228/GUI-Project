/******************************************************************************
* File Name: file_write_task.c
*
* Description: File write task implementation
*              - Receives recorded audio buffers from AudioRecordTask
*              - Generates WAV file headers
*              - Saves complete WAV files to SD card
*
*******************************************************************************/

#include "file_write_task.h"
#include "freertos_setup.h"
#include "audio_record_task.h"
#include "wav_file.h"
#include "FS.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************************
* Global Variables
*******************************************************************************/
TaskHandle_t file_write_task_handle = NULL;

/*******************************************************************************
* Local Variables
*******************************************************************************/
static uint32_t file_counter = 1;
static char filename_buffer[64];

/*******************************************************************************
* Function Name: generate_filename
********************************************************************************
* Summary:
*  Generate auto-incrementing filename
*
* Parameters:
*  None
*
* Return:
*  Pointer to filename string
*
*******************************************************************************/
static const char* generate_filename(void)
{
    snprintf(filename_buffer, sizeof(filename_buffer), "audio_%03u.wav", (unsigned int)file_counter);
    file_counter++;
    return filename_buffer;
}

/*******************************************************************************
* Function Name: file_write_task
********************************************************************************
* Summary:
*  File write task main loop
*  - Waits for audio_record_msg_t from audio_record_queue
*  - Generates WAV header based on sample count
*  - Saves WAV file to SD card (or simulates if SD not available)
*  - Notifies completion
*
* Parameters:
*  arg: Unused task parameter
*
*******************************************************************************/
static void file_write_task(void *arg)
{
    (void)arg;
    audio_record_msg_t record_msg;
    wav_header_t wav_header;
    const char *filename;
    
    /* Small delay to avoid printf collision with other tasks */
    vTaskDelay(pdMS_TO_TICKS(200));
    
    printf("\r\n=== File Write Task Started ===\r\n");
    
    for (;;)
    {
        /* Wait for recording data from AudioRecordTask (block indefinitely) */
        if (xQueueReceive(audio_record_queue, &record_msg, portMAX_DELAY) == pdPASS)
        {
            printf("[FileWriteTask] Received %u samples (%u channels, %u Hz)\r\n",
                   (unsigned int)record_msg.sample_count,
                   (unsigned int)record_msg.num_channels,
                   (unsigned int)record_msg.sample_rate);
            
            /* Generate filename */
            filename = generate_filename();
            
            /* Calculate audio duration */
            float duration_sec = (float)record_msg.sample_count / 
                                (float)(record_msg.sample_rate * record_msg.num_channels);
            
            printf("[FileWriteTask] Duration: %.2f seconds\r\n", duration_sec);
            printf("[FileWriteTask] Filename: %s\r\n", filename);
            
            /* Initialize WAV header (only needs total_samples parameter) */
            wav_header_init(&wav_header, record_msg.sample_count);
            
            printf("[FileWriteTask] WAV header generated (data_bytes=%u)\r\n",
                   (unsigned int)wav_header.data_bytes);
            
            /* Save to SD card using emFile */
            FS_FILE *file = FS_FOpen(filename, "w");
            if (file == NULL) {
                printf("[FileWriteTask] Error: Cannot create file '%s'\r\n", filename);
                continue;
            }
            
            /* Write WAV header */
            uint32_t written = FS_Write(file, &wav_header, WAV_HEADER_SIZE);
            if (written != WAV_HEADER_SIZE) {
                printf("[FileWriteTask] Error: Header write failed\r\n");
                FS_FClose(file);
                continue;
            }
            
            /* Write PCM data */
            uint32_t data_bytes = record_msg.sample_count * sizeof(int16_t);
            written = FS_Write(file, record_msg.buffer_ptr, data_bytes);
            if (written != data_bytes) {
                printf("[FileWriteTask] Error: Data write failed (%u/%u bytes)\r\n",
                       (unsigned int)written, (unsigned int)data_bytes);
                FS_FClose(file);
                continue;
            }
            
            FS_FClose(file);
            
            printf("[FileWriteTask] ✓ File saved: %s (%u bytes)\r\n",
                   filename, (unsigned int)(WAV_HEADER_SIZE + data_bytes));
            
            /* Notify completion (could set event flag or send message) */
            printf("[FileWriteTask] Write operation complete, ready for next recording\r\n");
            printf("---\r\n");
        }
    }
}

/*******************************************************************************
* Function Name: file_write_task_create
********************************************************************************
* Summary:
*  Create the file write task
*
*******************************************************************************/
void file_write_task_create(void)
{
    BaseType_t result = xTaskCreate(
        file_write_task,
        "FileWrite",
        FILE_WRITE_TASK_STACK_SIZE,
        NULL,
        FILE_WRITE_TASK_PRIORITY,
        &file_write_task_handle
    );
    
    if (result != pdPASS)
    {
        printf("Error: File Write Task creation failed\r\n");
        file_write_task_handle = NULL;
    }
}
