/*********************************************************************
*                     SEGGER Microcontroller GmbH                    *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*       (c) 2003 - 2023  SEGGER Microcontroller GmbH                 *
*                                                                    *
*       www.segger.com     Support: support_emfile@segger.com        *
*                                                                    *
**********************************************************************
*                                                                    *
*       emFile * File system for embedded applications               *
*                                                                    *
*                                                                    *
*       Please note:                                                 *
*                                                                    *
*       Knowledge of this file may under no circumstances            *
*       be used to write a similar product for in-house use.         *
*                                                                    *
*       Thank you for your fairness !                                *
*                                                                    *
**********************************************************************
*                                                                    *
*       emFile version: V5.22.0                                      *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
Licensing information
Licensor:                 SEGGER Microcontroller Systems LLC
Licensed to:              Cypress Semiconductor Corp, 198 Champion Ct., San Jose, CA 95134, USA
Licensed SEGGER software: emFile
License number:           FS-00227
License model:            Cypress Services and License Agreement, signed November 17th/18th, 2010
                          and Amendment Number One, signed December 28th, 2020 and February 10th, 2021
                          and Amendment Number Three, signed May 2nd, 2022 and May 5th, 2022
Licensed platform:        Any Cypress platform (Initial targets are: PSoC3, PSoC5, PSoC6)
----------------------------------------------------------------------
Support and Update Agreement (SUA)
SUA period:               2010-12-01 - 2023-07-27
Contact to extend SUA:    sales@segger.com
----------------------------------------------------------------------
File        : FS_ConfigMMC_CM_HS.c
Purpose     : Configuration functions for FS with MMC/SD card mode driver
              using 4-bit or 8-bit data bus and high speed access mode.
-------------------------- END-OF-HEADER -----------------------------
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include "FS.h"
#include "FS_MMC_HW_CM.h"
#include "cybsp.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define ALLOC_SIZE                 0x2000      // Size defined in bytes

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U32 _aMemBlock[ALLOC_SIZE / 4];        // Memory pool used for semi-dynamic allocation.
static mtb_hal_sdhc_t sdhcObj;

static FS_MMC_HW_CM_SDHostConfig_t SDConfig =
{
    .Obj = &sdhcObj,
    .IoVoltSelEn = true,
    .CardPwrEn = true,
};

/*********************************************************************
*
*       HW initialization weak function
*
**********************************************************************
*/
void FS_MMC_HW_CM_ConfigureHw(mtb_hal_sdhc_t *sdhcObj);

/**
 * @brief SDHC interrupt service routine wrapper.
 *
 * This wrapper function is needed because mtb_hal_sdhc_process_interrupt
 * has a parameter, but ISR cannot take parameters. The SDHC object
 * is obtained from the static sdhcObj variable.
 */
static void sdhc_isr_handler(void)
{
    mtb_hal_sdhc_process_interrupt(&sdhcObj);
}

/**
 * @brief Check if SD card is physically connected.
 *
 * This function reads the SD card detection GPIO pin to determine if
 * an SD card is inserted in the slot.
 *
 * @param base SDHC peripheral base address (unused)
 * @return bool True if SD card is detected, false otherwise
 */
bool Cy_SD_Host_IsCardConnected(SDHC_Type const *base)
{
    CY_UNUSED_PARAMETER(base);

    /* Read the SD card detect pin. The detection logic depends on the
     * board design (active high or active low). For most Cypress boards,
     * SD_DETECT is active low (card present = pin low).
     */
    return ((Cy_GPIO_Read(CYBSP_SDHC_DETECT_PORT, CYBSP_SDHC_DETECT_PIN)) ? false : true);
}

/*********************************************************************
*
*       FS_MMC_HW_CM_ConfigureHw
*
*  Function description
*    Call this function in FS_X_AddDevices. This function must
*    configure SD Card HW using PDL and HAL APIs.
*    
*    This implementation:
*    1. Enables and initializes SDHC peripheral via PDL
*    2. Initializes the SD card device
*    3. Sets up HAL (Hardware Abstraction Layer) object
*    4. Configures and enables SDHC interrupt
*/
void FS_MMC_HW_CM_ConfigureHw(mtb_hal_sdhc_t *sdhcObj)
{
    cy_rslt_t hal_status;
    cy_en_sd_host_status_t pdl_sdhc_status;
    cy_en_sysint_status_t  pdl_sysint_status;
    cy_stc_sd_host_context_t sdhc_host_context;

    /* Step 1: Enable SDHC peripheral */
    Cy_SD_Host_Enable(CYBSP_SDHC_1_HW);

    /* Step 2: Initialize SDHC peripheral using PDL */
    pdl_sdhc_status = Cy_SD_Host_Init(CYBSP_SDHC_1_HW, &CYBSP_SDHC_1_config, 
                                       &sdhc_host_context);
    if (CY_SD_HOST_SUCCESS != pdl_sdhc_status)
    {
        printf("[SDHC] ERROR: Cy_SD_Host_Init failed (status: %d)\r\n", 
               pdl_sdhc_status);
        return;
    }

    /* Step 3: Initialize the SD card */
    pdl_sdhc_status = Cy_SD_Host_InitCard(CYBSP_SDHC_1_HW, &CYBSP_SDHC_1_card_cfg, 
                                          &sdhc_host_context);
    if (CY_SD_HOST_SUCCESS != pdl_sdhc_status)
    {
        printf("[SDHC] ERROR: Cy_SD_Host_InitCard failed (status: %d)\r\n", 
               pdl_sdhc_status);
        return;
    }

    /* Step 4: Setup HAL (ModusToolbox abstraction layer) */
    hal_status = mtb_hal_sdhc_setup(sdhcObj, &CYBSP_SDHC_1_sdhc_hal_config, 
                                    NULL, &sdhc_host_context);
    if (CY_RSLT_SUCCESS != hal_status)
    {
        printf("[SDHC] ERROR: mtb_hal_sdhc_setup failed (status: 0x%x)\r\n", 
               (unsigned int)hal_status);
        return;
    }

    /* Step 5: Configure and enable SDHC interrupt */
    cy_stc_sysint_t sdhc_isr_config =
    {
        .intrSrc = CYBSP_SDHC_1_IRQ,
        .intrPriority = 3U,  /* SDHC interrupt priority */
    };

    pdl_sysint_status = Cy_SysInt_Init(&sdhc_isr_config, sdhc_isr_handler);
    if (CY_SYSINT_SUCCESS != pdl_sysint_status)
    {
        printf("[SDHC] ERROR: Cy_SysInt_Init failed (status: %d)\r\n", 
               pdl_sysint_status);
        return;
    }

    /* Enable NVIC interrupt */
    NVIC_EnableIRQ((IRQn_Type) sdhc_isr_config.intrSrc);

    printf("[SDHC] Hardware initialization completed successfully.\r\n");
}


/*********************************************************************
*
*       FS_X_AddDevices
*
*  Function description
*    This function is called by the FS during FS_Init().
*    It is supposed to add all devices, using primarily FS_AddDevice().
*
*  Note
*    (1) Other API functions may NOT be called, since this function is called
*        during initialization. The devices are not yet ready at this point.
*/
void FS_X_AddDevices(void) {
  //
  // Give file system memory to work with.
  //
  FS_AssignMemory(&_aMemBlock[0], sizeof(_aMemBlock));
  //
  // Add and configure the driver.
  //
  FS_AddDevice(&FS_MMC_CM_Driver);
  FS_MMC_CM_Allow4bitMode(0, 1);
  FS_MMC_CM_AllowHighSpeedMode(0, 1);

  /* Initialize HW here */
  FS_MMC_HW_CM_ConfigureHw(&sdhcObj);

  FS_MMC_HW_CM_Configure(0, &SDConfig);

  FS_MMC_CM_SetHWType(0, &FS_MMC_HW_CM);
  //
  // Configure the file system for fast write operations.
  //
#if FS_SUPPORT_FILE_BUFFER
  FS_ConfigFileBufferDefault(512, FS_FILE_BUFFER_WRITE);
#endif // FS_SUPPORT_FILE_BUFFER
  FS_SetFileWriteMode(FS_WRITEMODE_FAST);
}

/*********************************************************************
*
*       FS_X_GetTimeDate
*
*  Function description
*    Current time and date in a format suitable for the file system.
*
*  Additional information
*    Bit 0-4:   2-second count (0-29)
*    Bit 5-10:  Minutes (0-59)
*    Bit 11-15: Hours (0-23)
*    Bit 16-20: Day of month (1-31)
*    Bit 21-24: Month of year (1-12)
*    Bit 25-31: Count of years from 1980 (0-127)
*/
U32 FS_X_GetTimeDate(void) {
  U32 r;
  U16 Sec, Min, Hour;
  U16 Day, Month, Year;

  Sec   = 0;        // 0 based.  Valid range: 0..59
  Min   = 0;        // 0 based.  Valid range: 0..59
  Hour  = 0;        // 0 based.  Valid range: 0..23
  Day   = 1;        // 1 based.    Means that 1 is 1. Valid range is 1..31 (depending on month)
  Month = 1;        // 1 based.    Means that January is 1. Valid range is 1..12.
  Year  = 0;        // 1980 based. Means that 2007 would be 27.
  r   = Sec / 2 + (Min << 5) + (Hour  << 11);
  r  |= (U32)(Day + (Month << 5) + (Year  << 9)) << 16;
  return r;
}

/*************************** End of file ****************************/
