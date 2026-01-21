/******************************************************************************
* File Name: freertos_setup.h
*
* Description: FreeRTOS system setup and initialization
*
*******************************************************************************/

#ifndef __FREERTOS_SETUP_H__
#define __FREERTOS_SETUP_H__

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*******************************************************************************
* Macros - Event Group Bits
*******************************************************************************/
#define EVENT_IDLE              (1 << 0)
#define EVENT_RECORDING         (1 << 1)
#define EVENT_PLAYING           (1 << 2)
#define EVENT_SD_ERROR          (1 << 3)
#define EVENT_RECORDING_DONE    (1 << 4)
#define EVENT_PLAYBACK_DONE     (1 << 5)

/*******************************************************************************
* Global Variables - IPC Objects
*******************************************************************************/
extern EventGroupHandle_t audio_state_events;
extern SemaphoreHandle_t buffer_free_sem;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void freertos_system_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __FREERTOS_SETUP_H__ */
