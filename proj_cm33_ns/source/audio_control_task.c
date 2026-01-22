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
#include "file_read_task.h"
#include "FS.h"
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
    
    /* Clear idle state */
    xEventGroupClearBits(audio_state_events, EVENT_IDLE | EVENT_RECORDING_DONE);
    
    /* Signal AudioRecordTask to start recording */
    printf("Starting PDM recording...\r\n");
    xEventGroupSetBits(audio_state_events, EVENT_RECORDING);
    
    /* Update local state */
    recording_active = true;
    
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
    
    /* Signal AudioRecordTask to stop (clear EVENT_RECORDING) */
    xEventGroupClearBits(audio_state_events, EVENT_RECORDING);
    
    /* Update local state */
    recording_active = false;
    
    printf("[DEBUG] Stop signal sent to AudioRecordTask\r\n");
    
    /* Wait for recording completion (with timeout) */
    EventBits_t bits = xEventGroupWaitBits(
        audio_state_events,
        EVENT_RECORDING_DONE,
        pdTRUE,  /* Clear bit on exit */
        pdFALSE,
        pdMS_TO_TICKS(1000)  /* 1 second timeout */
    );
    
    if (bits & EVENT_RECORDING_DONE) {
        printf("Recording stopped successfully.\r\n");
        printf("(Data sent to FileWriteTask)\r\n");
    } else {
        printf("WARNING: Recording stop timeout\r\n");
    }
    
    printf("[DEBUG] Stop complete\r\n");
    
    /* Return to idle state */
    xEventGroupSetBits(audio_state_events, EVENT_IDLE);
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
    printf("Listing recent WAV files:\r\n");
    
    /* Check for recently created files */
    for (int i = 1; i <= 10; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "audio_%03d.wav", i);
        
        FS_FILE *file = FS_FOpen(filename, "r");
        if (file != NULL) {
            U32 size = FS_GetFileSize(file);
            FS_FClose(file);
            printf("  %s  (%u bytes)\r\n", filename, (unsigned int)size);
        }
    }
    
    printf("(Use 'play <filename>' to play a file)\r\n");
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
    file_read_msg_t read_msg;
    BaseType_t result;
    
    printf("Playing file: %s\r\n", filename);
    
    /* Clear idle state */
    xEventGroupClearBits(audio_state_events, EVENT_IDLE);
    
    /* Send filename to FileReadTask */
    strncpy(read_msg.filename, filename, sizeof(read_msg.filename) - 1);
    read_msg.filename[sizeof(read_msg.filename) - 1] = '\0';
    
    result = xQueueSend(file_read_queue, &read_msg, pdMS_TO_TICKS(100));
    if (result != pdPASS) {
        printf("Error: Failed to send read command\r\n");
        xEventGroupSetBits(audio_state_events, EVENT_IDLE);
        return;
    }
    
    printf("Read command sent to FileReadTask\r\n");
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
    int result;
    
    printf("Deleting file: %s\r\n", filename);
    
    /* Delete file from SD card */
    result = FS_Remove(filename);
    if (result == 0) {
        printf("File deleted successfully\r\n");
    } else {
        printf("Error: Failed to delete file (error %d)\r\n", result);
        printf("File may not exist or SD card is write-protected\r\n");
    }
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
    EventBits_t event_bits;
    
    printf("\r\n=== Audio Control Task Started ===\r\n");
    
    /* Set initial state to IDLE */
    xEventGroupSetBits(audio_state_events, EVENT_IDLE);
    
    /* Main command processing loop */
    while (1) {
        /* Wait for command from CLI (with timeout to check auto-stop) */
        if (xQueueReceive(audio_cmd_queue, &cmd_msg, pdMS_TO_TICKS(200)) == pdPASS) {
            
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
        
        /* Check for auto-stop (buffer full during recording) */
        if (recording_active) {
            event_bits = xEventGroupGetBits(audio_state_events);
            
            /* If recording done but user didn't manually stop */
            if (event_bits & EVENT_RECORDING_DONE) {
                printf("[AutoStop] Recording finished (buffer full)\r\n");
                
                /* Update state */
                recording_active = false;
                
                /* Clear done flag and return to idle */
                xEventGroupClearBits(audio_state_events, EVENT_RECORDING_DONE);
                xEventGroupSetBits(audio_state_events, EVENT_IDLE);
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
