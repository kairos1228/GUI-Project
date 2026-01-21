/******************************************************************************
* File Name: cli_task.h
*
* Description: UART CLI Task header file
*
*******************************************************************************/

#ifndef __CLI_TASK_H__
#define __CLI_TASK_H__

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*******************************************************************************
* Macros
*******************************************************************************/
#define CLI_TASK_STACK_SIZE         (1024u)
#define CLI_TASK_PRIORITY           (2u)
#define CLI_RX_QUEUE_LENGTH         (10u)
#define CLI_MAX_CMD_LENGTH          (64u)

/*******************************************************************************
* Enumerations
*******************************************************************************/
typedef enum {
    CMD_START_RECORD,
    CMD_LIST_FILES,
    CMD_PLAY_FILE,
    CMD_DELETE_FILE,
    CMD_UNKNOWN
} audio_cmd_t;

/*******************************************************************************
* Structures
*******************************************************************************/
typedef struct {
    audio_cmd_t cmd;
    char filename[32];
} audio_command_msg_t;

/*******************************************************************************
* Global Variables
*******************************************************************************/
extern QueueHandle_t cli_rx_queue;
extern QueueHandle_t audio_cmd_queue;
extern TaskHandle_t cli_task_handle;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void cli_task_create(void);
void cli_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_TASK_H__ */
