/******************************************************************************
* File Name: file_write_task.h
*
* Description: File write task - saves recorded audio to SD card as WAV files
*
*******************************************************************************/

#ifndef __FILE_WRITE_TASK_H__
#define __FILE_WRITE_TASK_H__

#include "FreeRTOS.h"
#include "task.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*******************************************************************************
* Macros
*******************************************************************************/
#define FILE_WRITE_TASK_PRIORITY    (3)
#define FILE_WRITE_TASK_STACK_SIZE  (2048)

/*******************************************************************************
* Global Variables
*******************************************************************************/
extern TaskHandle_t file_write_task_handle;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void file_write_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* __FILE_WRITE_TASK_H__ */
