/******************************************************************************
* File Name: audio_control_task.c
*
* Description: Audio Control Task - orchestrates recording, playback, and file operations
*
*******************************************************************************/

#include "audio_control_task.h"
#include "app_pdm_pcm.h"
#include "app_i2s.h"
#include "freertos_setup.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************************
* Global Variables
*******************************************************************************/
TaskHandle_t audio_control_task_handle = NULL;

/*******************************************************************************
* Local Variables
*******************************************************************************/
static bool recording_active = false;
static char current_filename[32] = "audio_001.wav";

/*******************************************************************************
* Function Name: handle_start_record
********************************************************************************
* Summary:
*  Start PDM recording
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void handle_start_record(void)
{
    /* Check if already recording */
    if (recording_active) {
        printf("Already recording. Stop first.\r\n");
        return;
    }
    
    /* Clear audio state events */
    xEventGroupClearBits(audio_state_events, EVENT_IDLE);
    
    /* Activate PDM recording */
    printf("Starting PDM recording...\r\n");
    app_pdm_pcm_activate();
    
    /* Update state */
    recording_active = true;
    xEventGroupSetBits(audio_state_events, EVENT_RECORDING);
    
    printf("Recording started. Type 'stop' to finish.\r\n");
}

/*******************************************************************************
* Function Name: handle_stop_record
********************************************************************************
* Summary:
*  Stop PDM recording and trigger file write
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void handle_stop_record(void)
{
    /* Check if recording is active */
    if (!recording_active) {
        printf("Not currently recording.\r\n");
        return;
    }
    
    printf("[DEBUG] Stopping recording...\r\n");
    
    /* Deactivate PDM */
    printf("Stopping PDM recording...\r\n");
    app_pdm_pcm_deactivate();
    
    printf("[DEBUG] PDM deactivated\r\n");
    
    /* Update state */
    recording_active = false;
    xEventGroupClearBits(audio_state_events, EVENT_RECORDING);
    
    /* TODO: Trigger FileWriteTask to save recorded_data[] to SD card */
    uint32_t sample_count = get_audio_data_index();
    printf("Recording stopped. %d samples captured.\r\n", (int)sample_count);
    printf("(File write task not yet implemented)\r\n");
    
    /* Return to idle state */
    xEventGroupSetBits(audio_state_events, EVENT_IDLE);
    
    printf("[DEBUG] Stop complete\r\n");
}

/*******************************************************************************
* Function Name: handle_list_files
********************************************************************************
* Summary:
*  List WAV files on SD card
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void handle_list_files(void)
{
    printf("Listing files...\r\n");
    
    /* TODO: Implement SD card file listing with emFile */
    printf("(SD card support not yet implemented)\r\n");
    printf("Example files:\r\n");
    printf("  audio_001.wav  (4 seconds, 16kHz stereo)\r\n");
    printf("  audio_002.wav  (2 seconds, 16kHz stereo)\r\n");
}

/*******************************************************************************
* Function Name: handle_play_file
********************************************************************************
* Summary:
*  Start playback of specified WAV file
*
* Parameters:
*  filename: Name of WAV file to play
*
* Return:
*  None
*
*******************************************************************************/
static void handle_play_file(const char *filename)
{
    printf("Playing file: %s\r\n", filename);
    
    /* Clear idle state */
    xEventGroupClearBits(audio_state_events, EVENT_IDLE);
    
    /* TODO: Send play command to PlaybackTask */
    printf("(Playback task not yet fully implemented)\r\n");
    
    /* For now, just return to idle */
    xEventGroupSetBits(audio_state_events, EVENT_IDLE);
}

/*******************************************************************************
* Function Name: handle_delete_file
********************************************************************************
* Summary:
*  Delete specified WAV file from SD card
*
* Parameters:
*  filename: Name of WAV file to delete
*
* Return:
*  None
*
*******************************************************************************/
static void handle_delete_file(const char *filename)
{
    printf("Deleting file: %s\r\n", filename);
    
    /* TODO: Implement file deletion with emFile FS_Remove() */
    printf("(SD card support not yet implemented)\r\n");
}

/*******************************************************************************
* Function Name: audio_control_task
********************************************************************************
* Summary:
*  Main Audio Control Task - processes commands from CLI and manages audio state
*
* Parameters:
*  pvParameters: Task parameters (unused)
*
* Return:
*  None (infinite loop)
*
*******************************************************************************/
void audio_control_task(void *pvParameters)
{
    (void)pvParameters;
    audio_command_msg_t cmd_msg;
    
    printf("\r\n=== Audio Control Task Started ===\r\n");
    
    /* Set initial state to IDLE */
    xEventGroupSetBits(audio_state_events, EVENT_IDLE);
    
    /* Main command processing loop */
    while (1) {
        /* Wait for command from CLI (blocking) */
        if (xQueueReceive(audio_cmd_queue, &cmd_msg, portMAX_DELAY) == pdPASS) {
            
            /* Process command */
            switch (cmd_msg.cmd) {
                case CMD_START_RECORD:
                    handle_start_record();
                    break;
                    
                case CMD_STOP_RECORD:
                    handle_stop_record();
                    break;
                    
                case CMD_LIST_FILES:
                    handle_list_files();
                    break;
                    
                case CMD_PLAY_FILE:
                    handle_play_file(cmd_msg.filename);
                    break;
                    
                case CMD_DELETE_FILE:
                    handle_delete_file(cmd_msg.filename);
                    break;
                    
                default:
                    printf("Unknown command received\r\n");
                    break;
            }
        }
    }
}

/*******************************************************************************
* Function Name: audio_control_task_create
********************************************************************************
* Summary:
*  Create Audio Control Task
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void audio_control_task_create(void)
{
    BaseType_t result;
    
    result = xTaskCreate(
        audio_control_task,                /* Task function */
        "AudioControl",                    /* Task name */
        AUDIO_CONTROL_TASK_STACK_SIZE,    /* Stack size */
        NULL,                              /* Parameters */
        AUDIO_CONTROL_TASK_PRIORITY,      /* Priority */
        &audio_control_task_handle         /* Task handle */
    );
    
    if (result != pdPASS) {
        printf("Error: Failed to create Audio Control Task\r\n");
        audio_control_task_handle = NULL;
    }
}
