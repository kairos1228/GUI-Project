/******************************************************************************
* File Name: FS_X_AddDevices.c
*
* Description: emFile device driver registration (minimal implementation)
*              Required weak function - must be defined by application
*
*******************************************************************************/

#include "FS.h"
#include "FS_MMC_HW_CM.h"
#include "cybsp.h"
#include <stdio.h>

/*******************************************************************************
* Constants
*******************************************************************************/
#define EMFILE_MEMORY_SIZE  (16384u)  /* 16KB for emFile heap */

/*******************************************************************************
* Static data
*******************************************************************************/
static U32 _aMemBlock[EMFILE_MEMORY_SIZE / 4];  /* emFile memory pool */
static mtb_hal_sdhc_t sdhcObj;                  /* HAL SDHC object */

/* SDHC configuration for emFile */
static FS_MMC_HW_CM_SDHostConfig_t SDConfig = {
    .Obj = &sdhcObj,
    .IoVoltSelEn = true,   /* Enable 1.8V I/O voltage selection */
    .CardPwrEn = true,     /* Enable card power control */
};

/*******************************************************************************
* Forward declarations
*******************************************************************************/
extern void FS_MMC_HW_CM_ConfigureHw(mtb_hal_sdhc_t *sdhcObj);

/*******************************************************************************
* Function Name: FS_X_AddDevices
********************************************************************************
* Summary:
*  Register SDHC device with emFile
*  Called automatically by FS_Init()
*  Minimal implementation - just register the driver
*
*******************************************************************************/
void FS_X_AddDevices(void)
{
    /* Assign memory pool to emFile */
    FS_AssignMemory(&_aMemBlock[0], sizeof(_aMemBlock));
    
    /* Add MMC/SD device driver */
    FS_AddDevice(&FS_MMC_CM_Driver);
    
    /* Enable 4-bit bus mode */
    FS_MMC_CM_Allow4bitMode(0, 1);
    
    /* Enable high-speed mode */
    FS_MMC_CM_AllowHighSpeedMode(0, 1);

    /* Initialize SDHC hardware */
    FS_MMC_HW_CM_ConfigureHw(&sdhcObj);

    /* Configure MMC driver with HAL object */
    FS_MMC_HW_CM_Configure(0, &SDConfig);

    /* Set hardware layer type */
    FS_MMC_CM_SetHWType(0, &FS_MMC_HW_CM);
}

/* [] END OF FILE */

