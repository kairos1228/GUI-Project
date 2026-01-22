/******************************************************************************
* File Name: sd_card_init.h
*
* Description: SD card initialization for emFile
*
*******************************************************************************/

#ifndef __SD_CARD_INIT_H__
#define __SD_CARD_INIT_H__

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
int sd_card_init(void);
void sd_card_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_CARD_INIT_H__ */
