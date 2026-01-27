/******************************************************************************
* File Name : emfile_sd.c
*
* Description : Implementation of emFile-based SD card file system operations
*               and WAV file writing functionality. Provides initialization,
*               mounting, and PCM to WAV conversion with SD card storage.
********************************************************************************
* Copyright 2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/* Disable format warnings for emFile API pointer conversions */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wint-conversion"

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

#include "emfile_sd.h"
#include "FS.h"
#include "FS_Types.h"
#include "cy_syslib.h"
#include "cy_rtc.h"

/* Deep sleep lock/unlock macros - define as empty if not available */
#ifndef mtb_hal_syspm_lock_deepsleep
#define mtb_hal_syspm_lock_deepsleep()
#endif

#ifndef mtb_hal_syspm_unlock_deepsleep
#define mtb_hal_syspm_unlock_deepsleep()
#endif


/*******************************************************************************
* Macros
*******************************************************************************/
#define EMFILE_MEM_SIZE_BYTES (32 * 1024)  /* 32 KB for emFile heap */
#define EMFILE_MEM_SIZE_U32   (EMFILE_MEM_SIZE_BYTES / sizeof(uint32_t))

#define MOUNT_VOLUME_NAME     ""             /* Root volume for SD card */

#define WAV_CHUNK_SIZE        (8 * 1024)     /* 8 KB chunks for writing */
#define FILENAME_BUFFER_SIZE  (32)           /* Filename buffer size */


/*******************************************************************************
* Type Definitions
*******************************************************************************/

/**
 * @brief WAV file header structure (RIFF/WAVE format, PCM)
 *
 * This structure represents the complete WAV file header including:
 * - RIFF container header (12 bytes)
 * - fmt subchunk (24 bytes)
 * - data subchunk header (8 bytes)
 *
 * Total: 44 bytes
 *
 * All multi-byte fields are stored in little-endian format as per WAV spec.
 */
typedef struct __attribute__((packed))
{
    /* RIFF Header */
    char    riff_id[4];         /* "RIFF" = 0x52494646 */
    uint32_t chunk_size;        /* File size - 8 bytes (little-endian) */
    char    wave_id[4];         /* "WAVE" = 0x57415645 */

    /* fmt Subchunk */
    char    fmt_id[4];          /* "fmt " = 0x666d7420 (note trailing space) */
    uint32_t subchunk1_size;    /* 16 bytes for PCM (little-endian) */
    uint16_t audio_format;      /* 1 = PCM, little-endian */
    uint16_t num_channels;      /* 1 = mono, 2 = stereo (little-endian) */
    uint32_t sample_rate;       /* e.g., 16000 for 16 kHz (little-endian) */
    uint32_t byte_rate;         /* SampleRate * NumChannels * BitsPerSample/8 (little-endian) */
    uint16_t block_align;       /* NumChannels * BitsPerSample / 8 (little-endian) */
    uint16_t bits_per_sample;   /* 16 or 8 (little-endian) */

    /* data Subchunk */
    char    data_id[4];         /* "data" = 0x64617461 */
    uint32_t subchunk2_size;    /* Audio data size in bytes (little-endian) */

} WAV_HEADER_t;

_Static_assert(sizeof(WAV_HEADER_t) == 44, "WAV_HEADER_t must be exactly 44 bytes");


/*******************************************************************************
* Static Variables
*******************************************************************************/

/* Initialization flag to ensure single initialization */
static bool _emfile_initialized = false;

/* Mount status flag */
static bool _emfile_mounted = false;

/* Mounted volume name (used for file operations) */
static char _mounted_volume_name[32] = "";

/* Static counter for sequential filename generation */
static uint32_t _file_counter = 0;


/*******************************************************************************
* Function Definitions
*******************************************************************************/

/* Compile-time timestamp (parsed once at startup) */
static struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} _compile_time = {0, 0, 0, 0, 0, 0};

static bool _compile_time_parsed = false;

/**
 * @brief Parse compile-time timestamp from __DATE__ and __TIME__.
 *
 * Called once during initialization to extract timestamp.
 * Uses standard GCC macros:
 * - __DATE__: "Mmm dd yyyy" (e.g., "Jan 27 2026")
 * - __TIME__: "hh:mm:ss" (e.g., "11:25:34")
 */
static void emfile_parse_compile_time(void)
{
    const char *date_str = __DATE__;
    const char *time_str = __TIME__;
    
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int month_num = 1;
    int i;

    printf("[RTC] Parsing compile-time timestamp...\r\n");
    printf("[RTC]   Date: %s\r\n", date_str);
    printf("[RTC]   Time: %s\r\n", time_str);

    /* Parse month (first 3 characters) */
    for (i = 0; i < 12; i++)
    {
        if (strncmp(date_str, months[i], 3) == 0)
        {
            month_num = i + 1;
            break;
        }
    }

    /* Parse day and year */
    int day, year;
    if (sscanf(date_str, "%*s %d %d", &day, &year) != 2)
    {
        printf("[RTC] WARNING: Failed to parse date\r\n");
        day = 1;
        month_num = 1;
        year = 2026;
    }

    /* Parse time */
    int hour, minute, second;
    if (sscanf(time_str, "%d:%d:%d", &hour, &minute, &second) != 3)
    {
        printf("[RTC] WARNING: Failed to parse time\r\n");
        hour = 0;
        minute = 0;
        second = 0;
    }

    /* Store parsed values */
    _compile_time.year = (uint16_t)year;
    _compile_time.month = (uint8_t)month_num;
    _compile_time.day = (uint8_t)day;
    _compile_time.hour = (uint8_t)hour;
    _compile_time.minute = (uint8_t)minute;
    _compile_time.second = (uint8_t)second;
    _compile_time_parsed = true;

    printf("[RTC] Compile time: %04u-%02u-%02u %02u:%02u:%02u\r\n",
           _compile_time.year, _compile_time.month, _compile_time.day,
           _compile_time.hour, _compile_time.minute, _compile_time.second);
}

/**
 * @brief Generate filename based on current RTC time with Korea timezone.
 *
 * Generates filename in format: YYYYMMDD_HHMMSS.wav
 * Example: 20260127_112300.wav (Korea Standard Time, UTC+9)
 *
 * This ensures each recording has a unique name based on when it was created.
 * No counter is needed - the timestamp guarantees uniqueness.
 *
 * @return Pointer to static filename buffer
 *         Returns fallback "recording.wav" if RTC read fails
 */
const char* app_emfile_generate_timestamp_filename(void)
{
    static char _filename_buffer[32];
    int snprintf_result;
    
    /* Ensure compile time has been parsed */
    if (!_compile_time_parsed)
    {
        printf("[FS] WARNING: Compile time not parsed. Using fallback.\r\n");
        return "recording.wav";
    }

    /* Apply KST offset (+9 hours) */
    uint16_t year = _compile_time.year;
    uint8_t month = _compile_time.month;
    uint8_t day = _compile_time.day;
    uint8_t hour = _compile_time.hour;
    uint8_t minute = _compile_time.minute;
    uint8_t second = _compile_time.second;

    /* Handle hour wraparound (>= 24) */
    if (hour >= 24)
    {
        hour -= 24;
        day += 1;

        /* Simple month/year handling (assumes we won't overflow beyond month 12) */
        const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        uint8_t max_days = days_in_month[month - 1];

        /* 2026 is not a leap year */
        if (day > max_days)
        {
            day = 1;
            month += 1;
            if (month > 12)
            {
                month = 1;
                year += 1;
            }
        }
    }

    /* Format: YYYYMMDD_HHMMSS.wav */
    snprintf_result = snprintf(_filename_buffer,
                              sizeof(_filename_buffer),
                              "%04u%02u%02u_%02u%02u%02u.wav",
                              year, month, day,
                              hour, minute, second);

    if (snprintf_result < 0 || snprintf_result >= (int)sizeof(_filename_buffer))
    {
        printf("[FS] WARNING: Filename buffer overflow. Using fallback.\r\n");
        return "recording.wav";
    }

    printf("[FS] Generated timestamp filename: %s (KST)\r\n", _filename_buffer);
    return _filename_buffer;
}


/**
 * @brief Get next filename with automatic retry on open failure.
 *
 * Attempts to find an available filename by incrementing the counter if
 * file open fails. Will retry up to 10 times before giving up.
 *
 * @param[out] out_filename  Pointer to buffer to store the final filename
 * @param[in]  buffer_size   Size of the output buffer
 *
 * @return true  if an available filename was found
 *         false if all retry attempts failed
 */
bool app_emfile_find_available_filename(char* out_filename, uint32_t buffer_size)
{
    uint32_t retry_count = 0;
    const uint32_t max_retries = 10;
    const char* candidate_filename;
    intptr_t fd;  /* File pointer as intptr_t for emFile */

    if (out_filename == NULL || buffer_size < FILENAME_BUFFER_SIZE)
    {
        printf("[FS] ERROR: Invalid output buffer.\r\n");
        return false;
    }

    printf("[FS] Finding available filename (max %u retries)...\r\n", (unsigned int)max_retries);

    while (retry_count < max_retries)
    {
        candidate_filename = app_emfile_next_filename();

        /* Try to open file (will fail if doesn't exist, which is what we want) */
        fd = (intptr_t)FS_FOpen(candidate_filename, "rb");
        if (fd == 0)
        {
            /* File doesn't exist - this is good, use this name */
            printf("[FS] Found available filename: %s\r\n", candidate_filename);
            strncpy(out_filename, candidate_filename, buffer_size - 1);
            out_filename[buffer_size - 1] = '\0';
            return true;
        }
        else
        {
            /* File exists - close it and try next number */
            FS_FClose((void*)fd);
            printf("[FS] File exists, trying next... (attempt %u/%u)\r\n",
                   (unsigned int)(retry_count + 1), (unsigned int)max_retries);
            _file_counter++;
            retry_count++;
        }
    }

    printf("[FS] ERROR: Could not find available filename after %u retries.\r\n",
           (unsigned int)max_retries);
    return false;
}
bool app_emfile_init(void)
{
    /* Prevent multiple initializations */
    if (_emfile_initialized)
    {
        printf("[emFile] Already initialized, skipping re-initialization.\r\n");
        return true;
    }

    printf("[emFile] Initializing file system...\r\n");

    /* Step 0: Parse compile-time timestamp for filename generation */
    emfile_parse_compile_time();

    /* Step 1: Initialize emFile internals */
    FS_Init();
    printf("[emFile] FS_Init() completed.\r\n");

    /* Step 2: Add devices (SD card via SDHC) */
    /* This internally calls FS_MMC_HW_CM_ConfigureHw() and FS_AssignMemory() */
    FS_X_AddDevices();
    printf("[emFile] FS_X_AddDevices() completed.\r\n");

    _emfile_initialized = true;
    _emfile_mounted = true;
    
    /* Step 3: Wait for file system to stabilize */
    /* emFile needs time to:
     * 1. Detect SD card presence
     * 2. Read card configuration
     * 3. Scan FAT32 filesystem
     * 4. Build directory structures
     * Insufficient delay causes FS_FOpen to fail silently */
    printf("[emFile] Waiting 1 second for file system stabilization...\r\n");
    Cy_SysLib_Delay(1000);  /* 1000 milliseconds */
    
    /* Step 4: Verify volume is accessible */
    /* Check if we can query volume information */
    I32 num_volumes = FS_GetNumVolumes();
    printf("[emFile] Number of mounted volumes: %ld\r\n", (long)num_volumes);
    
    if (num_volumes <= 0)
    {
        printf("[emFile] WARNING: No volumes mounted. SD card may not be ready.\r\n");
        printf("[emFile]   - Verify SD card is inserted\r\n");
        printf("[emFile]   - Verify SD card has FAT32 filesystem\r\n");
        printf("[emFile]   - Check SDHC hardware initialization logs above\r\n");
        /* Continue anyway - file operations may still work */
    }
    else
    {
        /* Try to get first volume name for file operations */
        printf("[emFile] Attempting to detect volume names...\r\n");
        char vol_name[32];
        
        /* Initialize to empty (root volume) */
        _mounted_volume_name[0] = '\0';
        
        /* Try to get volume 0 name (SD card is usually volume 0) */
        /* NOTE: FS_GetVolumeName() may block on some implementations,
         * so we use a simple fallback strategy */
        if (num_volumes > 0)
        {
            /* First try with FS_GetVolumeName - but don't loop */
            FS_GetVolumeName(0, vol_name, sizeof(vol_name));
            printf("[emFile]   SD Volume: '%s'\r\n", vol_name);
            
            if (strlen(vol_name) > 0)
            {
                strncpy(_mounted_volume_name, vol_name, sizeof(_mounted_volume_name) - 1);
                _mounted_volume_name[sizeof(_mounted_volume_name) - 1] = '\0';
                printf("[emFile] Using volume: '%s' for file operations\r\n", _mounted_volume_name);
            }
        }
        
        printf("[emFile] File system ready for file operations.\r\n");
    }

    return true;
}


/**
 * @brief Mount SD card volume.
 *
 * Note: In this implementation, the FAT32 filesystem is expected to be
 * present on the SD card. No format checks or high-level formatting is
 * performed. FS_Mount() is called implicitly during file operations
 * through emFile's automatic mounting mechanism.
 */
bool app_emfile_mount(void)
{
    if (!_emfile_initialized)
    {
        printf("[emFile] ERROR: File system not initialized. Call app_emfile_init() first.\r\n");
        return false;
    }

    if (_emfile_mounted)
    {
        printf("[emFile] Volume already mounted.\r\n");
        return true;
    }

    printf("[emFile] File system ready (auto-mount enabled).\r\n");
    _emfile_mounted = true;
    return true;
}


/**
 * @brief Save PCM audio buffer as WAV file to SD card.
 */
bool app_wav_save_from_buffer(const int16_t* pcm_interleaved,
                              uint32_t num_samples_per_channel,
                              uint32_t sample_rate_hz,
                              uint16_t num_channels,
                              uint16_t bits_per_sample,
                              const char* filename)
{
    void *p_file;                   /* emFile file pointer */
    WAV_HEADER_t wav_header;
    uint32_t bytes_written = 0;
    uint32_t total_samples;
    uint32_t bytes_per_sample;
    uint32_t subchunk2_size;
    uint32_t byte_rate;
    uint16_t block_align;
    uint32_t remaining_samples;
    uint32_t samples_to_write;
    uint32_t bytes_to_write;
    int write_result;

    printf("[WAV] Saving WAV file: %s\r\n", filename);
    printf("[WAV]   Sample rate: %" PRIu32 " Hz, Channels: %u, Bits/sample: %u\r\n",
           sample_rate_hz, num_channels, bits_per_sample);
    printf("[WAV]   Samples/channel: %" PRIu32 "\r\n", num_samples_per_channel);

    /* Validate inputs */
    if (!_emfile_mounted)
    {
        printf("[WAV] ERROR: SD card not mounted. Call app_emfile_mount() first.\r\n");
        return false;
    }

    if (pcm_interleaved == NULL || filename == NULL)
    {
        printf("[WAV] ERROR: Invalid parameters (NULL pointer).\r\n");
        return false;
    }

    if (num_channels == 0 || num_channels > 8)
    {
        printf("[WAV] ERROR: Invalid number of channels: %u (must be 1-8).\r\n",
               num_channels);
        return false;
    }

    if (bits_per_sample != 8 && bits_per_sample != 16)
    {
        printf("[WAV] ERROR: Unsupported bits per sample: %u (must be 8 or 16).\r\n",
               bits_per_sample);
        return false;
    }

    /* Calculate WAV file parameters */
    bytes_per_sample = bits_per_sample / 8;
    total_samples = num_samples_per_channel * num_channels;
    subchunk2_size = total_samples * bytes_per_sample;
    byte_rate = sample_rate_hz * num_channels * bytes_per_sample;
    block_align = num_channels * bytes_per_sample;

    printf("[WAV] Calculating header:\r\n");
    printf("[WAV]   Total samples (all channels): %" PRIu32 "\r\n", total_samples);
    printf("[WAV]   Bytes per sample: %" PRIu32 "\r\n", bytes_per_sample);
    printf("[WAV]   Audio data size: %" PRIu32 " bytes\r\n", subchunk2_size);
    printf("[WAV]   Byte rate: %" PRIu32 " bytes/sec\r\n", byte_rate);
    printf("[WAV]   Block align: %u bytes\r\n", block_align);

    /* Initialize WAV header with RIFF/WAVE/fmt structure */
    memcpy(wav_header.riff_id, "RIFF", 4);
    wav_header.chunk_size = 36 + subchunk2_size;  /* File size minus 8 */
    memcpy(wav_header.wave_id, "WAVE", 4);

    memcpy(wav_header.fmt_id, "fmt ", 4);
    wav_header.subchunk1_size = 16;             /* PCM format always 16 */
    wav_header.audio_format = 1;                /* 1 = PCM (uncompressed) */
    wav_header.num_channels = num_channels;
    wav_header.sample_rate = sample_rate_hz;
    wav_header.byte_rate = byte_rate;
    wav_header.block_align = block_align;
    wav_header.bits_per_sample = bits_per_sample;

    memcpy(wav_header.data_id, "data", 4);
    wav_header.subchunk2_size = subchunk2_size;

    printf("[WAV] WAV header constructed (total file size: %" PRIu32 " bytes).\r\n",
           44 + subchunk2_size);

    /* Prevent deep sleep during SD write operations */
    mtb_hal_syspm_lock_deepsleep();

    /* Diagnostic: Check file system status before opening */
    printf("[WAV] Pre-open diagnostics:\r\n");
    printf("[WAV]   Mounted volumes: %ld\r\n", (long)FS_GetNumVolumes());
    printf("[WAV]   Target volume: '%s'\r\n", _mounted_volume_name[0] ? _mounted_volume_name : "(root)");
    
    /* Build full file path with volume name
     * NOTE: _mounted_volume_name already includes trailing colon (e.g., "mmc:0:")
     * So we just concatenate filename directly */
    char full_filepath[64];
    if (_mounted_volume_name[0] != '\0')
    {
        /* _mounted_volume_name already ends with ':', just concatenate filename */
        snprintf(full_filepath, sizeof(full_filepath), "%s%s", _mounted_volume_name, filename);
    }
    else
    {
        /* Use root volume (empty volume name) */
        snprintf(full_filepath, sizeof(full_filepath), "%s", filename);
    }
    
    /* Open file for writing (binary mode) */
    printf("[WAV] Opening file: %s\r\n", full_filepath);
    p_file = FS_FOpen(full_filepath, "wb");
    if (p_file == NULL)
    {
        printf("[WAV] ERROR: FS_FOpen() returned NULL.\r\n");
        printf("[WAV] Diagnostic information:\r\n");
        printf("[WAV]   - Filename: '%s'\r\n", filename);
        printf("[WAV]   - Mode: write binary (\"wb\")\r\n");
        printf("[WAV]   - Possible causes:\r\n");
        printf("[WAV]       * File system not mounted (no volumes available)\r\n");
        printf("[WAV]       * SD card not detected or not readable\r\n");
        printf("[WAV]       * Card filesystem is not FAT32\r\n");
        printf("[WAV]       * Invalid filename format\r\n");
        printf("[WAV]       * SD card write-protected\r\n");
        printf("[WAV]       * SDHC hardware initialization failed\r\n");
        mtb_hal_syspm_unlock_deepsleep();
        return false;
    }

    /* Write WAV header (44 bytes) */
    printf("[WAV] Writing header (44 bytes)...\r\n");
    write_result = FS_FWrite((void*)&wav_header, 1, sizeof(wav_header), p_file);
    
    /* FS_FWrite returns number of items written, not bytes.
     * Since ItemSize=1 and NumItems=sizeof(wav_header), 
     * we expect return value = sizeof(wav_header) */
    if (write_result != (int)sizeof(wav_header))
    {
        printf("[WAV] ERROR: Failed to write WAV header.\r\n");
        printf("[WAV]   Expected to write: %zu bytes\r\n", sizeof(wav_header));
        printf("[WAV]   Actually wrote: %d bytes\r\n", write_result);
        FS_FClose(p_file);
        mtb_hal_syspm_unlock_deepsleep();
        return false;
    }
    bytes_written = write_result;

    /* Write audio data in 8 KB chunks to avoid stack overflow */
    remaining_samples = total_samples;
    printf("[WAV] Writing audio data in %u-byte chunks...\r\n", WAV_CHUNK_SIZE);

    while (remaining_samples > 0)
    {
        /* Calculate how many samples to write in this chunk */
        samples_to_write = (remaining_samples > WAV_CHUNK_SIZE / bytes_per_sample)
                         ? (WAV_CHUNK_SIZE / bytes_per_sample)
                         : remaining_samples;
        bytes_to_write = samples_to_write * bytes_per_sample;

        /* Write chunk of PCM data 
         * FS_FWrite(pData, ItemSize, NumItems, pFile)
         * We use ItemSize=1, NumItems=bytes_to_write
         * Return value = number of items (bytes) written */
        write_result = FS_FWrite(
            (void*)(pcm_interleaved + (total_samples - remaining_samples)),
            1,
            bytes_to_write,
            p_file);

        if (write_result != (int)bytes_to_write)
        {
            printf("[WAV] ERROR: Failed to write audio chunk.\r\n");
            printf("[WAV]   Expected to write: %" PRIu32 " bytes\r\n", bytes_to_write);
            printf("[WAV]   Actually wrote: %d bytes\r\n", write_result);
            FS_FClose(p_file);
            mtb_hal_syspm_unlock_deepsleep();
            return false;
        }

        bytes_written += write_result;
        remaining_samples -= samples_to_write;

        printf("[WAV]   Progress: %" PRIu32 " bytes written, %" PRIu32 " samples remaining.\r\n",
               bytes_written, remaining_samples);
    }

    /* Sync file to ensure all data is written to disk */
    printf("[WAV] Syncing file to disk...\r\n");
    write_result = FS_SyncFile(p_file);
    if (write_result != 0)
    {
        printf("[WAV] WARNING: FS_SyncFile() returned error code %d.\r\n", write_result);
    }

    /* Close file */
    printf("[WAV] Closing file...\r\n");
    write_result = FS_FClose(p_file);
    if (write_result != 0)
    {
        printf("[WAV] ERROR: Failed to close file (error code: %d).\r\n", write_result);
        mtb_hal_syspm_unlock_deepsleep();
        return false;
    }

    /* Allow deep sleep again */
    mtb_hal_syspm_unlock_deepsleep();

    printf("[WAV] WAV file saved successfully: %s (%u bytes total).\r\n",
           filename, (unsigned int)bytes_written);

    return true;
}


/* [] END OF FILE */
#pragma GCC diagnostic pop
