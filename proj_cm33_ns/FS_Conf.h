/*********************************************************************
*                     SEGGER Microcontroller GmbH                    *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*       (c) 2003 - 2021  SEGGER Microcontroller GmbH                *
*                                                                    *
*       www.segger.com     Support: support_emfile@segger.com       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : FS_Conf.h
Purpose : File system configuration
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef FS_CONF_H
#define FS_CONF_H

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       File system configuration
*/
#define FS_SUPPORT_FAT              1       /* Required for SD card FAT32 */
#define FS_SUPPORT_EFS              0       /* Embedded File System (not needed) */

/*********************************************************************
*
*       Operating system configuration
*/
#define FS_OS_LOCKING               1       /* Enable thread safety (FreeRTOS) */
#define FS_OS_EMBOS                 0       /* Not using embOS */

/*********************************************************************
*
*       Device driver configuration
*/
#define FS_SUPPORT_FILE_BUFFER      1       /* Enable file buffering for performance */

/*********************************************************************
*
*       Memory configuration
*/
#define FS_SECTOR_BUFFER_SIZE       512     /* SD card sector size */

/*********************************************************************
*
*       Cache configuration
*/
#define FS_SUPPORT_CACHE            1       /* Enable caching */
#define FS_MAX_LEN_FULL_FILE_NAME   256     /* Max file path length */

/*********************************************************************
*
*       Debug configuration
*/
#define FS_DEBUG_LEVEL              0       /* 0=No debug, 2=Warnings, 3=Logs */

/*********************************************************************
*
*       Volume configuration
*/
#define FS_SUPPORT_MULTIPLE_FS      0       /* Single file system (FAT only) */
#define FS_SUPPORT_ENCRYPTION       0       /* No encryption */
#define FS_SUPPORT_JOURNAL          0       /* No journaling (for simplicity) */

/*********************************************************************
*
*       Performance configuration
*/
#define FS_SUPPORT_BUSY_LED         0       /* No busy LED indicator */
#define FS_MAINTAIN_FAT_COPY        1       /* Maintain FAT copies for reliability */

#endif /* FS_CONF_H */

/*************************** End of file ****************************/
