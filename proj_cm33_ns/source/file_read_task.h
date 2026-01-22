/******************************************************************************
* File Name: file_read_task.h
*
* Description: WAV file reading task header
*              Reads WAV files from SD card and sends PCM data to PlaybackTask
*
*******************************************************************************/

#ifndef __FILE_READ_TASK_H__
#define __FILE_READ_TASK_H__

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
#define FILE_READ_TASK_STACK_SIZE    (2048u)
#define FILE_READ_TASK_PRIORITY      (3u)
#define PCM_CHUNK_SIZE               (4096u)  /* Samples per chunk */

/*******************************************************************************
* Structures
*******************************************************************************/
/* Message to FileReadTask (filename to read) */
typedef struct {
    char filename[32];
} file_read_msg_t;

/* Message to PlaybackTask (PCM data chunk) */
typedef struct {
    int16_t *buffer_ptr;      /* Pointer to PCM buffer */
    uint32_t sample_count;    /* Number of samples (stereo: L+R counted as 2) */
    bool is_last_chunk;       /* True if this is the final chunk */
} pcm_playback_msg_t;

/*******************************************************************************
* Global Variables
*******************************************************************************/
extern QueueHandle_t file_read_queue;         /* AudioControl → FileRead */
extern QueueHandle_t pcm_playback_queue;      /* FileRead → Playback */
extern TaskHandle_t file_read_task_handle;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void file_read_task_create(void);
void file_read_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* __FILE_READ_TASK_H__ */
