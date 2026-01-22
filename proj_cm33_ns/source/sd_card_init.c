/******************************************************************************
* File Name: sd_card_init.c
*
* Description: SD card initialization for emFile
*              Initializes emFile file system and mounts SD card volume
*
*******************************************************************************/

#include "sd_card_init.h"
#include "FS.h"
#include <stdio.h>

/*******************************************************************************
* Macros
*******************************************************************************/
#define VOLUME_NAME         ""      /* Empty string for default volume */
#define SD_CARD_DEVICE      "mmc:0:0"  /* MMC device */

/*******************************************************************************
* Function Name: sd_card_init
********************************************************************************
* Summary:
*  Initialize emFile and mount SD card
*
* Return:
*  0 on success, -1 on error
*
*******************************************************************************/
int sd_card_init(void)
{
    int error;
    U32 volume_size;
    
    printf("\r\n=== SD Card File System Initialization ===\r\n");
    
    /* Initialize emFile (automatically calls FS_X_AddDevices) */
    printf("Initializing emFile...\r\n");
    FS_Init();
    printf("✓ emFile initialized\r\n\r\n");
    
    /* Check if the volume needs high-level formatting */
    printf("Checking if volume is high-level formatted...\r\n");
    error = FS_IsHLFormatted(VOLUME_NAME);
    
    /* A return value of 0 indicates that high-level formatting is required */
    if (0U == error) {
        printf("⚠️  Volume not formatted. Performing high-level formatting...\r\n");
        error = FS_Format(VOLUME_NAME, NULL);
        
        if (error < 0) {
            printf("\r\n");
            printf("========================================\r\n");
            printf("  ❌ SD Card Format Failed\r\n");
            printf("========================================\r\n");
            printf("Error code: %d (%s)\r\n", error, FS_ErrorNo2Text(error));
            printf("\r\n");
            printf("Possible causes:\r\n");
            printf("  - SD card is damaged or write-protected\r\n");
            printf("  - Hardware initialization failed\r\n");
            printf("\r\n");
            printf("System will run in SRAM-only mode (no file storage).\r\n");
            printf("========================================\r\n\r\n");
            return -1;  /* Non-fatal - allow system to continue */
        }
        printf("✓ Volume formatted successfully\r\n\r\n");
    }
    
    /* Get volume size (triggers automatic mounting) */
    printf("Getting volume information...\r\n");
    volume_size = FS_GetVolumeSizeKB(VOLUME_NAME);
    
    if (0U == volume_size) {
        printf("❌ Error: Unable to read volume size\r\n");
        printf("System will run in SRAM-only mode (no file storage).\r\n\r\n");
        return -1;  /* Non-fatal - allow system to continue */
    }
    
    printf("✓ Volume size: %u KB\r\n", (unsigned int)volume_size);
    
    /* Get volume free space */
    U32 free_space = FS_GetVolumeFreeSpace(VOLUME_NAME);
    if (free_space != 0) {
        printf("✓ Volume free space: %u KB\r\n", (unsigned int)free_space);
    }
    
    printf("=== SD Card Ready ===\r\n\r\n");
    
    return 0;
}

/*******************************************************************************
* Function Name: sd_card_deinit
********************************************************************************
* Summary:
*  Unmount SD card and deinitialize emFile
*
*******************************************************************************/
void sd_card_deinit(void)
{
    FS_Unmount(VOLUME_NAME);
    printf("SD card unmounted\r\n");
}
