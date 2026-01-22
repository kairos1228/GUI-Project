/******************************************************************************
* File Name: file_read_task.c
*
* Description: WAV file reading task implementation
*              Reads WAV files from SD card and streams PCM data to PlaybackTask
*
*******************************************************************************/

#include "file_read_task.h"
#include "wav_file.h"
#include "FS.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************************
* Global Variables
*******************************************************************************/
QueueHandle_t file_read_queue = NULL;
QueueHandle_t pcm_playback_queue = NULL;
TaskHandle_t file_read_task_handle = NULL;

/*******************************************************************************
* Local Variables
*******************************************************************************/
/* PCM data buffers for streaming (ping-pong) */
static int16_t read_ping_buffer[PCM_CHUNK_SIZE] __attribute__((aligned(4)));
static int16_t read_pong_buffer[PCM_CHUNK_SIZE] __attribute__((aligned(4)));

/*******************************************************************************
* Function Name: parse_wav_header
********************************************************************************
* Summary:
*  Parse and validate WAV file header
*
* Parameters:
*  file: Opened file handle
*  total_samples: Output - total samples (stereo counted as 2)
*
* Return:
*  0 on success, -1 on error
*
*******************************************************************************/
static int parse_wav_header(FS_FILE *file, uint32_t *total_samples)
{
    wav_header_t header;
    uint32_t bytes_read;
    
    /* Read WAV header */
    bytes_read = FS_Read(file, &header, WAV_HEADER_SIZE);
    if (bytes_read != WAV_HEADER_SIZE) {
        printf("[FileReadTask] Error: Failed to read WAV header\r\n");
        return -1;
    }
    
    /* Validate RIFF header */
    if (header.riff_header[0] != 'R' || header.riff_header[1] != 'I' ||
        header.riff_header[2] != 'F' || header.riff_header[3] != 'F') {
        printf("[FileReadTask] Error: Invalid RIFF header\r\n");
        return -1;
    }
    
    /* Validate WAVE header */
    if (header.wave_header[0] != 'W' || header.wave_header[1] != 'A' ||
        header.wave_header[2] != 'V' || header.wave_header[3] != 'E') {
        printf("[FileReadTask] Error: Invalid WAVE header\r\n");
        return -1;
    }
    
    /* Validate PCM format */
    if (header.audio_format != 1) {
        printf("[FileReadTask] Error: Only PCM format supported (format=%d)\r\n", 
               header.audio_format);
        return -1;
    }
    
    /* Validate sample rate */
    if (header.sample_rate != WAV_SAMPLE_RATE) {
        printf("[FileReadTask] Warning: Sample rate %u Hz (expected %d Hz)\r\n", 
               (unsigned int)header.sample_rate, WAV_SAMPLE_RATE);
    }
    
    /* Validate channels */
    if (header.num_channels != WAV_NUM_CHANNELS) {
        printf("[FileReadTask] Error: Expected %d channels, got %d\r\n",
               WAV_NUM_CHANNELS, header.num_channels);
        return -1;
    }
    
    /* Validate bit depth */
    if (header.bits_per_sample != WAV_BITS_PER_SAMPLE) {
        printf("[FileReadTask] Error: Expected %d-bit, got %d-bit\r\n",
               WAV_BITS_PER_SAMPLE, header.bits_per_sample);
        return -1;
    }
    
    /* Calculate total samples (L+R counted separately) */
    *total_samples = header.data_bytes / sizeof(int16_t);
    
    printf("[FileReadTask] WAV: %u Hz, %d ch, %d bit, %u samples\r\n",
           (unsigned int)header.sample_rate, header.num_channels, 
           header.bits_per_sample, (unsigned int)*total_samples);
    
    return 0;
}

/*******************************************************************************
* Function Name: file_read_task
********************************************************************************
* Summary:
*  File reading task - reads WAV from SD and sends PCM chunks to PlaybackTask
*
* Parameters:
*  pvParameters: Task parameters (unused)
*
* Return:
*  None
*
*******************************************************************************/
void file_read_task(void *pvParameters)
{
    (void)pvParameters;
    file_read_msg_t msg;
    pcm_playback_msg_t pcm_msg;
    FS_FILE *file;
    uint32_t total_samples;
    uint32_t samples_read;
    uint32_t samples_remaining;
    int16_t *current_buffer;
    bool using_ping;
    
    /* Add startup delay */
    vTaskDelay(pdMS_TO_TICKS(350));
    
    printf("=== File Read Task Started ===\r\n");
    
    while (1) {
        /* Wait for read command (filename) */
        if (xQueueReceive(file_read_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        printf("[FileReadTask] Opening '%s'...\r\n", msg.filename);
        
        /* Open WAV file from SD card */
        file = FS_FOpen(msg.filename, "r");
        if (file == NULL) {
            printf("[FileReadTask] Error: Cannot open '%s'\r\n", msg.filename);
            continue;
        }
        
        /* Parse WAV header */
        if (parse_wav_header(file, &total_samples) != 0) {
            FS_FClose(file);
            printf("[FileReadTask] Error: Invalid WAV file\r\n");
            continue;
        }
        
        samples_remaining = total_samples;
        using_ping = true;
        
        /* Stream file in chunks */
        while (samples_remaining > 0) {
            /* Select buffer */
            current_buffer = using_ping ? read_ping_buffer : read_pong_buffer;
            
            /* Determine chunk size */
            uint32_t chunk_samples = (samples_remaining < PCM_CHUNK_SIZE) ? 
                                      samples_remaining : PCM_CHUNK_SIZE;
            
            /* Read PCM data from SD */
            samples_read = FS_Read(file, current_buffer, chunk_samples * sizeof(int16_t));
            samples_read /= sizeof(int16_t);
            
            if (samples_read == 0) {
                printf("[FileReadTask] Warning: Read 0 samples (EOF)\r\n");
                break;
            }
            
            /* Prepare PCM message */
            pcm_msg.buffer_ptr = current_buffer;
            pcm_msg.sample_count = samples_read;
            pcm_msg.is_last_chunk = (samples_remaining <= samples_read);
            
            /* Send to PlaybackTask */
            if (xQueueSend(pcm_playback_queue, &pcm_msg, pdMS_TO_TICKS(500)) != pdPASS) {
                printf("[FileReadTask] Error: Failed to send PCM chunk\r\n");
                break;
            }
            
            samples_remaining -= samples_read;
            using_ping = !using_ping;  /* Ping-pong buffer swap */
        }
        
        /* Close file */
        FS_FClose(file);
        
        printf("[FileReadTask] File read complete\r\n");
    }
}

/*******************************************************************************
* Function Name: file_read_task_create
********************************************************************************
* Summary:
*  Create File Read Task
*
*******************************************************************************/
void file_read_task_create(void)
{
    BaseType_t result = xTaskCreate(
        file_read_task,
        "FileRead",
        FILE_READ_TASK_STACK_SIZE,
        NULL,
        FILE_READ_TASK_PRIORITY,
        &file_read_task_handle
    );
    
    if (result != pdPASS) {
        printf("Error: File Read Task creation failed\r\n");
        file_read_task_handle = NULL;
    }
}
