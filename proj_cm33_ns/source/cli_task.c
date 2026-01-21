/******************************************************************************
* File Name: cli_task.c
*
* Description: UART CLI Task implementation
*
*******************************************************************************/

#include "cli_task.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************************
* Global Variables
*******************************************************************************/
QueueHandle_t cli_rx_queue = NULL;
QueueHandle_t audio_cmd_queue = NULL;
TaskHandle_t cli_task_handle = NULL;

/*******************************************************************************
* Local Variables
*******************************************************************************/
static char cmd_buffer[CLI_MAX_CMD_LENGTH];

/*******************************************************************************
* Function Name: cli_parse_command
********************************************************************************
* Summary:
*  Parse received command string and create audio command message
*
* Parameters:
*  cmd_str: Input command string
*  msg: Output audio command message
*
* Return:
*  true if command is valid, false otherwise
*
*******************************************************************************/
static bool cli_parse_command(const char *cmd_str, audio_command_msg_t *msg)
{
    char cmd[16];
    char arg[32];
    int num_parsed;
    
    /* Clear message structure */
    memset(msg, 0, sizeof(audio_command_msg_t));
    msg->cmd = CMD_UNKNOWN;
    
    /* Parse command and optional argument */
    num_parsed = sscanf(cmd_str, "%15s %31s", cmd, arg);
    
    if (num_parsed < 1) {
        return false;
    }
    
    /* Match command */
    if (strcmp(cmd, "record") == 0) {
        msg->cmd = CMD_START_RECORD;
        return true;
    }
    else if (strcmp(cmd, "ls") == 0) {
        msg->cmd = CMD_LIST_FILES;
        return true;
    }
    else if (strcmp(cmd, "play") == 0) {
        if (num_parsed >= 2) {
            msg->cmd = CMD_PLAY_FILE;
            strncpy(msg->filename, arg, sizeof(msg->filename) - 1);
            msg->filename[sizeof(msg->filename) - 1] = '\0';
            return true;
        }
        else {
            printf("Usage: play <filename>\r\n");
            return false;
        }
    }
    else if (strcmp(cmd, "rm") == 0) {
        if (num_parsed >= 2) {
            msg->cmd = CMD_DELETE_FILE;
            strncpy(msg->filename, arg, sizeof(msg->filename) - 1);
            msg->filename[sizeof(msg->filename) - 1] = '\0';
            return true;
        }
        else {
            printf("Usage: rm <filename>\r\n");
            return false;
        }
    }
    else {
        printf("Unknown command: %s\r\n", cmd);
        printf("Available commands:\r\n");
        printf("  record          - Start recording\r\n");
        printf("  ls              - List files\r\n");
        printf("  play <filename> - Play WAV file\r\n");
        printf("  rm <filename>   - Delete file\r\n");
        return false;
    }
}

/*******************************************************************************
* Function Name: cli_task
********************************************************************************
* Summary:
*  CLI Task main loop - receives UART commands and dispatches to audio task
*
* Parameters:
*  pvParameters: Task parameters (unused)
*
* Return:
*  None
*
*******************************************************************************/
void cli_task(void *pvParameters)
{
    (void)pvParameters;
    char rx_char;
    uint32_t cmd_index = 0;
    audio_command_msg_t cmd_msg;
    
    printf("\r\n=== Audio Recorder CLI ===\r\n");
    printf("Type 'help' for command list\r\n");
    printf("> ");
    fflush(stdout);
    
    while (1) {
        /* Wait for character from UART RX queue */
        if (xQueueReceive(cli_rx_queue, &rx_char, portMAX_DELAY) == pdTRUE) {
            
            /* Echo character */
            printf("%c", rx_char);
            fflush(stdout);
            
            /* Handle backspace */
            if (rx_char == '\b' || rx_char == 0x7F) {
                if (cmd_index > 0) {
                    cmd_index--;
                    printf(" \b");  /* Erase character on terminal */
                    fflush(stdout);
                }
                continue;
            }
            
            /* Handle newline (command complete) */
            if (rx_char == '\r' || rx_char == '\n') {
                printf("\r\n");
                
                if (cmd_index > 0) {
                    cmd_buffer[cmd_index] = '\0';
                    
                    /* Parse and send command */
                    if (cli_parse_command(cmd_buffer, &cmd_msg)) {
                        if (xQueueSend(audio_cmd_queue, &cmd_msg, 0) != pdTRUE) {
                            printf("Error: Command queue full\r\n");
                        }
                    }
                    
                    /* Reset buffer */
                    cmd_index = 0;
                }
                
                printf("> ");
                fflush(stdout);
                continue;
            }
            
            /* Add character to buffer */
            if (cmd_index < (CLI_MAX_CMD_LENGTH - 1)) {
                cmd_buffer[cmd_index++] = rx_char;
            }
            else {
                printf("\r\nError: Command too long\r\n> ");
                fflush(stdout);
                cmd_index = 0;
            }
        }
    }
}

/*******************************************************************************
* Function Name: cli_task_create
********************************************************************************
* Summary:
*  Create CLI task and necessary queues
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void cli_task_create(void)
{
    /* Create UART RX queue (receives characters from UART ISR) */
    cli_rx_queue = xQueueCreate(CLI_RX_QUEUE_LENGTH, sizeof(char));
    if (cli_rx_queue == NULL) {
        printf("Error: Failed to create CLI RX queue\r\n");
        return;
    }
    
    /* Create audio command queue (sends commands to audio control task) */
    audio_cmd_queue = xQueueCreate(5, sizeof(audio_command_msg_t));
    if (audio_cmd_queue == NULL) {
        printf("Error: Failed to create audio command queue\r\n");
        return;
    }
    
    /* Create CLI task */
    if (xTaskCreate(cli_task,
                    "CLI_Task",
                    CLI_TASK_STACK_SIZE,
                    NULL,
                    CLI_TASK_PRIORITY,
                    &cli_task_handle) != pdPASS) {
        printf("Error: Failed to create CLI task\r\n");
    }
}
