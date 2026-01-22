/******************************************************************************
* File Name: sdhc_init.c
*
* Description: SDHC hardware initialization for emFile
*              Based on working example: test-emfile-2
*
* Hardware:
*  - SDHC1 peripheral (4-bit mode, ADMA2 DMA)
*  - Interrupt-driven data transfer
*
******************************************************************************/

#include "cy_sd_host.h"
#include "cy_sysint.h"
#include "cy_gpio.h"
#include "cybsp.h"
#include "mtb_hal_sdhc.h"
#include <stdio.h>

/*******************************************************************************
* Constants
*******************************************************************************/
#define SDHC_IRQ_PRIORITY   (7u)       /* NVIC interrupt priority */
#define SD_CARD_PRESENT     (0u)       /* Card detect pin LOW = card inserted */

/*******************************************************************************
* Global variables
*******************************************************************************/
cy_stc_sd_host_context_t sdhc_host_context;  /* PDL SDHC context */
static mtb_hal_sdhc_t *sdhc_app_obj = NULL;  /* HAL SDHC object pointer */

/*******************************************************************************
* Function Name: Cy_SD_Host_IsCardConnected
********************************************************************************
* Summary:
*  Override weak function in PSoC PDL to provide custom card detection
*  P17[7] = LOW when card inserted, HIGH when card removed
*
* Parameters:
*  base: SDHC hardware base address
*
* Return:
*  true  - Card is connected
*  false - Card is not connected
*
*******************************************************************************/
bool Cy_SD_Host_IsCardConnected(SDHC_Type const *base)
{
    (void)base;  /* Unused parameter */
    
    /* Read card detect pin P17[7]
     * Returns 0 (LOW) when card is inserted
     * Returns 1 (HIGH) when card is removed */
    uint32_t pin_state = Cy_GPIO_Read(CYBSP_SDHC_DETECT_PORT, CYBSP_SDHC_DETECT_PIN);
    
    return (SD_CARD_PRESENT == pin_state);
}

/*******************************************************************************
* Function Name: sd_card_isr
********************************************************************************
* Summary:
*  SDHC interrupt handler
*  Forwards interrupt to HAL driver
*
*******************************************************************************/
static void sd_card_isr(void)
{
    if (sdhc_app_obj != NULL) {
        mtb_hal_sdhc_process_interrupt(sdhc_app_obj);
    }
}

/*******************************************************************************
* Function Name: FS_MMC_HW_CM_ConfigureHw
********************************************************************************
* Summary:
*  Initialize SDHC1 hardware using PSoC PDL and HAL
*  Called from FS_X_AddDevices() during emFile initialization
*
* Parameters:
*  sdhcObj: Pointer to HAL SDHC object (must persist)
*
* Note:
*  This is a weak function that can be overridden by user application
*
*******************************************************************************/
__attribute__((weak)) void FS_MMC_HW_CM_ConfigureHw(mtb_hal_sdhc_t *sdhcObj)
{
    cy_rslt_t hal_status;
    cy_en_sd_host_status_t pdl_sdhc_status;
    cy_en_sysint_status_t pdl_sysint_status;
    bool card_detected;

    /* Store HAL object pointer for ISR */
    sdhc_app_obj = sdhcObj;

    /* Configure SDHC interrupt */
    cy_stc_sysint_t sdhc_isr_config = {
        .intrSrc = CYBSP_SDHC_1_IRQ,
        .intrPriority = SDHC_IRQ_PRIORITY,
    };

    printf("  [1/8] Checking for SD card presence...\r\n");
    card_detected = Cy_SD_Host_IsCardConnected(CYBSP_SDHC_1_HW);
    printf("        Card detect pin (P17[7]): %s\r\n", 
           card_detected ? "INSERTED ✓" : "NOT INSERTED ✗");
    
    if (!card_detected) {
        printf("  ERROR: No SD card detected!\r\n");
        printf("         Please insert SD card into J35 connector and restart.\r\n");
        CY_ASSERT(0);
    }

    printf("  [2/8] Enabling SDHC peripheral...\r\n");
    Cy_SD_Host_Enable(CYBSP_SDHC_1_HW);

    printf("  [3/8] Initializing SD Host controller (PDL)...\r\n");
    pdl_sdhc_status = Cy_SD_Host_Init(CYBSP_SDHC_1_HW, 
                                      &CYBSP_SDHC_1_config, 
                                      &sdhc_host_context);
    if (CY_SD_HOST_SUCCESS != pdl_sdhc_status) {
        printf("  ERROR: Cy_SD_Host_Init failed: %d\r\n", pdl_sdhc_status);
        printf("  System halted. Check SDHC hardware configuration.\r\n");
        CY_ASSERT(0);
    }

    printf("  [4/8] Waiting for card power stabilization...\r\n");
    Cy_SysLib_Delay(200);  /* 200ms delay for card power-up (increased) */

    printf("  [5/8] Initializing SD card...\r\n");
    
    /* Try up to 3 times with increasing delays */
    for (int retry = 0; retry < 3; retry++) {
        if (retry > 0) {
            printf("        Retry attempt %d/2...\r\n", retry);
            Cy_SysLib_Delay(500);  /* Wait 500ms before retry */
        }
        
        pdl_sdhc_status = Cy_SD_Host_InitCard(CYBSP_SDHC_1_HW, 
                                              &CYBSP_SDHC_1_card_cfg, 
                                              &sdhc_host_context);
        
        if (CY_SD_HOST_SUCCESS == pdl_sdhc_status) {
            printf("        ✓ Card initialized successfully\r\n");
            break;  /* Success! */
        }
        
        printf("        Failed with error: %d (0x%X)\r\n", pdl_sdhc_status, pdl_sdhc_status);
    }
    
    if (CY_SD_HOST_SUCCESS != pdl_sdhc_status) {
        printf("  ERROR: Cy_SD_Host_InitCard failed after 3 attempts: %d (0x%X)\r\n", 
               pdl_sdhc_status, pdl_sdhc_status);
        printf("  Possible causes:\r\n");
        printf("    - SD card is damaged or incompatible\r\n");
        printf("    - Try a different SD card (SDHC Class 10, 4-32GB)\r\n");
        printf("    - Ensure card is FAT32 formatted\r\n");
        printf("    - Check card is fully inserted in J35\r\n");
        printf("  System halted.\r\n");
        CY_ASSERT(0);
    }

    printf("  [6/8] Setting up HAL SDHC object...\r\n");
    hal_status = mtb_hal_sdhc_setup(sdhcObj, 
                                    &CYBSP_SDHC_1_sdhc_hal_config, 
                                    NULL, 
                                    &sdhc_host_context);
    if (CY_RSLT_SUCCESS != hal_status) {
        printf("  ERROR: mtb_hal_sdhc_setup failed: 0x%08X\r\n", (unsigned int)hal_status);
        printf("  System halted.\r\n");
        CY_ASSERT(0);
    }

    printf("  [5/7] Initializing SDHC interrupt...\r\n");
    pdl_sysint_status = Cy_SysInt_Init(&sdhc_isr_config, sd_card_isr);
    if (CY_SYSINT_SUCCESS != pdl_sysint_status) {
        printf("  ERROR: Cy_SysInt_Init failed: %d\r\n", pdl_sysint_status);
        printf("  System halted.\r\n");
        CY_ASSERT(0);
    }

    printf("  [6/7] Enabling NVIC interrupt...\r\n");
    NVIC_EnableIRQ((IRQn_Type)sdhc_isr_config.intrSrc);

    printf("  [7/7] SDHC hardware initialization complete!\r\n");
}
