/******************************************************************************
* File Name: playback_task.h
*
* Description: WAV file playback task header
*
*******************************************************************************/

#ifndef __PLAYBACK_TASK_H__
#define __PLAYBACK_TASK_H__

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
#define PLAYBACK_TASK_STACK_SIZE    (2048u)
#define PLAYBACK_TASK_PRIORITY      (3u)
#define PLAYBACK_CHUNK_SIZE         (4096u)  /* Samples per buffer */

/*******************************************************************************
* Structures
*******************************************************************************/
typedef struct {
    char filename[32];
} playback_msg_t;

/*******************************************************************************
* Global Variables
*******************************************************************************/
extern QueueHandle_t playback_queue;
extern TaskHandle_t playback_task_handle;
extern volatile int16_t *playback_buffer_ptr;  /* Pointer for I2S ISR */
extern volatile uint32_t playback_samples_remaining;
extern volatile bool playback_active;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void playback_task_create(void);
void playback_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* __PLAYBACK_TASK_H__ */
