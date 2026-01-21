/******************************************************************************
* File Name: audio_control_task.h
*
* Description: Audio Control Task header - central coordinator for recording/playback
*
*******************************************************************************/

#ifndef __AUDIO_CONTROL_TASK_H__
#define __AUDIO_CONTROL_TASK_H__

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "cli_task.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*******************************************************************************
* Macros
*******************************************************************************/
#define AUDIO_CONTROL_TASK_STACK_SIZE   (2048u)
#define AUDIO_CONTROL_TASK_PRIORITY     (3u)  /* Higher than CLI */

/* Event Group Bits - Audio System State */
#define EVENT_IDLE              (1 << 0)  /* System idle */
#define EVENT_RECORDING         (1 << 1)  /* Recording in progress */
#define EVENT_PLAYING           (1 << 2)  /* Playback in progress */
#define EVENT_FILE_WRITING      (1 << 3)  /* Writing WAV file */
#define EVENT_ERROR             (1 << 4)  /* Error occurred */

/*******************************************************************************
* Global Variables
*******************************************************************************/
extern TaskHandle_t audio_control_task_handle;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void audio_control_task_create(void);
void audio_control_task(void *pvParameters);

#if defined(__cplusplus)
}
#endif

#endif /* __AUDIO_CONTROL_TASK_H__ */
