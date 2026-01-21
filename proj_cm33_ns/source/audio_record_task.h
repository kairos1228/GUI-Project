/******************************************************************************
* File Name: audio_record_task.h
*
* Description: Audio recording task - manages PDM capture and buffer monitoring
*
*******************************************************************************/

#ifndef __AUDIO_RECORD_TASK_H__
#define __AUDIO_RECORD_TASK_H__

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*******************************************************************************
* Macros
*******************************************************************************/
#define AUDIO_RECORD_TASK_PRIORITY    (4)
#define AUDIO_RECORD_TASK_STACK_SIZE  (1024)
#define AUDIO_RECORD_QUEUE_LENGTH     (2)

/*******************************************************************************
* Data Structures
*******************************************************************************/
typedef struct {
    int16_t *buffer_ptr;      /* Pointer to recorded audio buffer */
    uint32_t sample_count;    /* Number of samples (total, not per channel) */
    uint32_t sample_rate;     /* Sampling rate in Hz */
    uint16_t num_channels;    /* Number of audio channels */
} audio_record_msg_t;

/*******************************************************************************
* Global Variables
*******************************************************************************/
extern TaskHandle_t audio_record_task_handle;
/* audio_record_queue is declared in freertos_setup.h */

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void audio_record_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_RECORD_TASK_H__ */
