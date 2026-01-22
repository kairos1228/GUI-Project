/******************************************************************************
* File Name: FS_X_GetTimeDate.c
*
* Description: emFile time/date callback implementation
*              Provides timestamp for file operations
*
*******************************************************************************/

#include "FS.h"

/*******************************************************************************
* Function Name: FS_X_GetTimeDate
********************************************************************************
* Summary:
*  Get current time and date as 32-bit timestamp
*  Called by emFile when creating/modifying files
*
* Parameters:
*  None
*
* Return:
*  U32 timestamp (DOS format: bits 0-4=sec/2, 5-10=min, 11-15=hour, 
*                 16-20=day, 21-24=month, 25-31=year-1980)
*
*******************************************************************************/
U32 FS_X_GetTimeDate(void)
{
    FS_FILETIME filetime;
    U32 timestamp;
    
    /* Set fixed time: 2026-01-21 12:00:00
     * In production, replace with RTC or system tick-based time
     */
    filetime.Year   = 2026;
    filetime.Month  = 1;
    filetime.Day    = 21;
    filetime.Hour   = 12;
    filetime.Minute = 0;
    filetime.Second = 0;
    
    /* Convert to DOS timestamp format */
    FS_FileTimeToTimeStamp(&filetime, &timestamp);
    
    return timestamp;
}
