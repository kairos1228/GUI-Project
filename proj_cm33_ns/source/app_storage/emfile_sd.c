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

/* Static counter for sequential filename generation */
static uint32_t _file_counter = 0;

/* Static buffer for filename */
#define FILENAME_BUFFER_SIZE (32)
static char _filename_buffer[FILENAME_BUFFER_SIZE];


/*******************************************************************************
* Function Definitions
*******************************************************************************/

/**
 * @brief Generate next filename for WAV recording.
 *
 * Generates sequential filenames in the format "rec_XXXX.wav" where XXXX is
 * a zero-padded 4-digit counter (0000-9999). Uses a static counter that
 * increments after each call.
 *
 * @note The returned pointer is valid until the next call to this function,
 *       as it uses a static buffer.
 *
 * @note For timestamped filenames, integrate with RTC:
 *       - Call Cy_RTC_GetDateAndTime() to get current date/time
 *       - Format as "rec_YYYYMMDD_HHMMSS.wav" (e.g., "rec_20260126_143022.wav")
 *       - This would prevent overwrites and provide chronological sorting
 *
 * @return Pointer to static filename buffer (e.g., "rec_0001.wav")
 *         The buffer is valid until the next call to this function.
 */
const char* app_emfile_next_filename(void)
{
    int snprintf_result;

    /* Generate filename with current counter value */
    snprintf_result = snprintf(_filename_buffer,
                              FILENAME_BUFFER_SIZE,
                              "rec_%04" PRIu32 ".wav",
                              _file_counter);

    if (snprintf_result < 0 || snprintf_result >= FILENAME_BUFFER_SIZE)
    {
        printf("[FS] WARNING: Filename buffer overflow or formatting error.\r\n");
        return "rec_0000.wav";  /* Fallback */
    }

    printf("[FS] Generated filename: %s (counter: %" PRIu32 ")\r\n",
           _filename_buffer, _file_counter);

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

    /* Step 1: Initialize emFile internals */
    FS_Init();
    printf("[emFile] FS_Init() completed.\r\n");

    /* Step 2: Add devices (SD card via SDHC) */
    /* This internally calls FS_MMC_HW_CM_ConfigureHw() and FS_AssignMemory() */
    FS_X_AddDevices();
    printf("[emFile] FS_X_AddDevices() completed.\r\n");

    _emfile_initialized = true;
    
    /* Step 3: Automatically mark as mounted since emFile auto-mounts through FS_X_AddDevices */
    /* The volume is accessible immediately after device initialization */
    _emfile_mounted = true;
    
    printf("[emFile] File system initialized successfully.\r\n");
    printf("[emFile] Note: SD card must have FAT32 filesystem.\r\n");

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

    /* Open file for writing (binary mode) */
    printf("[WAV] Opening file...\r\n");
    p_file = (void*)FS_FOpen(filename, "wb");
    if (p_file == NULL || (intptr_t)p_file == 0)
    {
        printf("[WAV] ERROR: Failed to open file for writing.\r\n");
        mtb_hal_syspm_unlock_deepsleep();
        return false;
    }

    /* Write WAV header (44 bytes) */
    printf("[WAV] Writing header (44 bytes)...\r\n");
    write_result = FS_FWrite((void*)&wav_header, 1, sizeof(wav_header), p_file);
    if (write_result != sizeof(wav_header))
    {
        printf("[WAV] ERROR: Failed to write WAV header (wrote %d bytes, expected %zu).\r\n",
               write_result, sizeof(wav_header));
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

        /* Write chunk of PCM data */
        write_result = FS_FWrite(
            (void*)(pcm_interleaved + (total_samples - remaining_samples)),
            1,
            bytes_to_write,
            p_file);

        if (write_result != bytes_to_write)
        {
            printf("[WAV] ERROR: Failed to write audio chunk (wrote %d bytes, expected %" PRIu32 ").\r\n",
                   write_result, bytes_to_write);
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
        printf("[WAV] ERROR: Failed to close file (error code %d).\r\n", write_result);
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
