/******************************************************************************
* File Name: playback_task.c
*
* Description: WAV file playback task implementation
*
*******************************************************************************/

#include "playback_task.h"
#include "wav_file.h"
#include "app_i2s.h"
#include "FS.h"
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
/* Ping-pong buffers for streaming */
static int16_t playback_ping_buffer[PLAYBACK_CHUNK_SIZE] __attribute__((aligned(4)));
static int16_t playback_pong_buffer[PLAYBACK_CHUNK_SIZE] __attribute__((aligned(4)));

/*******************************************************************************
* Function Name: playback_parse_wav_header
********************************************************************************
* Summary:
*  Parse WAV file header and validate format
*
* Parameters:
*  file: Opened file pointer
*  total_samples: Output - total number of samples per channel
*
* Return:
*  0 on success, -1 on error
*
*******************************************************************************/
static int playback_parse_wav_header(FS_FILE *file, uint32_t *total_samples)
{
    wav_header_t header;
    uint32_t bytes_read;
    
    /* Read WAV header */
    bytes_read = FS_Read(file, &header, WAV_HEADER_SIZE);
    if (bytes_read != WAV_HEADER_SIZE) {
        printf("Error: Failed to read WAV header\r\n");
        return -1;
    }
    
    /* Validate RIFF header */
    if (header.riff_header[0] != 'R' || header.riff_header[1] != 'I' ||
        header.riff_header[2] != 'F' || header.riff_header[3] != 'F') {
        printf("Error: Invalid RIFF header\r\n");
        return -1;
    }
    
    /* Validate WAVE header */
    if (header.wave_header[0] != 'W' || header.wave_header[1] != 'A' ||
        header.wave_header[2] != 'V' || header.wave_header[3] != 'E') {
        printf("Error: Invalid WAVE header\r\n");
        return -1;
    }
    
    /* Validate format (PCM only) */
    if (header.audio_format != 1) {
        printf("Error: Only PCM format supported (got format %d)\r\n", header.audio_format);
        return -1;
    }
    
    /* Validate sample rate */
    if (header.sample_rate != WAV_SAMPLE_RATE) {
        printf("Warning: Sample rate mismatch (expected %d, got %lu)\r\n", 
               WAV_SAMPLE_RATE, header.sample_rate);
    }
    
    /* Validate channels */
    if (header.num_channels != WAV_NUM_CHANNELS) {
        printf("Error: Channel mismatch (expected %d, got %d)\r\n", 
               WAV_NUM_CHANNELS, header.num_channels);
        return -1;
    }
    
    /* Validate bit depth */
    if (header.bits_per_sample != WAV_BITS_PER_SAMPLE) {
        printf("Error: Bit depth mismatch (expected %d, got %d)\r\n", 
               WAV_BITS_PER_SAMPLE, header.bits_per_sample);
        return -1;
    }
    
    /* Calculate total samples per channel */
    *total_samples = header.data_bytes / (WAV_NUM_CHANNELS * (WAV_BITS_PER_SAMPLE / 8));
    
    printf("WAV info: %lu Hz, %d ch, %d bit, %lu samples/ch\r\n",
           header.sample_rate, header.num_channels, header.bits_per_sample, *total_samples);
    
    return 0;
}

/*******************************************************************************
* Function Name: playback_task
********************************************************************************
* Summary:
*  Playback task - reads WAV file from SD card and streams to I2S
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
    playback_msg_t msg;
    FS_FILE *file;
    uint32_t total_samples;
    uint32_t samples_read;
    uint32_t samples_to_read;
    int16_t *current_buffer;
    int16_t *next_buffer;
    bool using_ping;
    
    while (1) {
        /* Wait for playback command */
        if (xQueueReceive(playback_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        printf("Playback: Opening %s...\r\n", msg.filename);
        
        /* Open WAV file */
        file = FS_FOpen(msg.filename, "r");
        if (file == NULL) {
            printf("Error: Cannot open file %s\r\n", msg.filename);
            continue;
        }
        
        /* Parse and validate WAV header */
        if (playback_parse_wav_header(file, &total_samples) != 0) {
            FS_FClose(file);
            printf("Error: Invalid WAV file\r\n");
            continue;
        }
        
        /* Initialize ping-pong buffers */
        using_ping = true;
        current_buffer = playback_ping_buffer;
        next_buffer = playback_pong_buffer;
        
        /* Read first chunk into ping buffer */
        samples_to_read = (total_samples < PLAYBACK_CHUNK_SIZE) ? total_samples : PLAYBACK_CHUNK_SIZE;
        samples_read = FS_Read(file, current_buffer, samples_to_read * sizeof(int16_t));
        samples_read /= sizeof(int16_t);
        
        if (samples_read == 0) {
            printf("Error: No data read from file\r\n");
            FS_FClose(file);
            continue;
        }
        
        /* Set up I2S playback */
        playback_buffer_ptr = current_buffer;
        playback_samples_remaining = samples_read;
        playback_active = true;
        
        /* Enable I2S (assumes I2S driver already initialized) */
        app_i2s_enable();
        app_i2s_activate();
        
        printf("Playback started (%lu samples)...\r\n", total_samples);
        
        total_samples -= samples_read;
        
        /* Streaming loop */
        while (total_samples > 0 && playback_active) {
            /* Read next chunk into inactive buffer */
            samples_to_read = (total_samples < PLAYBACK_CHUNK_SIZE) ? total_samples : PLAYBACK_CHUNK_SIZE;
            samples_read = FS_Read(file, next_buffer, samples_to_read * sizeof(int16_t));
            samples_read /= sizeof(int16_t);
            
            if (samples_read == 0) {
                break;  /* End of file or read error */
            }
            
            /* Wait for I2S ISR to finish current buffer (with timeout) */
            uint32_t timeout_count = 0;
            while (playback_samples_remaining > 0 && timeout_count < 1000) {
                vTaskDelay(pdMS_TO_TICKS(10));
                timeout_count++;
            }
            
            if (timeout_count >= 1000) {
                printf("Warning: Playback timeout\r\n");
                break;
            }
            
            /* Swap buffers (critical section) */
            taskENTER_CRITICAL();
            playback_buffer_ptr = next_buffer;
            playback_samples_remaining = samples_read;
            taskEXIT_CRITICAL();
            
            /* Swap ping-pong */
            if (using_ping) {
                current_buffer = playback_pong_buffer;
                next_buffer = playback_ping_buffer;
            } else {
                current_buffer = playback_ping_buffer;
                next_buffer = playback_pong_buffer;
            }
            using_ping = !using_ping;
            
            total_samples -= samples_read;
        }
        
        /* Wait for final buffer to finish */
        while (playback_samples_remaining > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        /* Stop I2S */
        playback_active = false;
        app_i2s_deactivate();
        app_i2s_disable();
        
        /* Close file */
        FS_FClose(file);
        
        printf("Playback completed\r\n");
    }
}

/*******************************************************************************
* Function Name: playback_task_create
********************************************************************************
* Summary:
*  Create playback task and queue
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void playback_task_create(void)
{
    /* Create playback command queue */
    playback_queue = xQueueCreate(2, sizeof(playback_msg_t));
    if (playback_queue == NULL) {
        printf("Error: Failed to create playback queue\r\n");
        return;
    }
    
    /* Create playback task */
    if (xTaskCreate(playback_task,
                    "Playback_Task",
                    PLAYBACK_TASK_STACK_SIZE,
                    NULL,
                    PLAYBACK_TASK_PRIORITY,
                    &playback_task_handle) != pdPASS) {
        printf("Error: Failed to create playback task\r\n");
    }
}
