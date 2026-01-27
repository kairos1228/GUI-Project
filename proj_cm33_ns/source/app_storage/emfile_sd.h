/******************************************************************************
* File Name : emfile_sd.h
*
* Description : Header file for emFile-based SD card operations and WAV file writing.
*               Provides API for file system initialization, mounting, and
*               PCM audio buffer to WAV file conversion and storage.
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
#ifndef __EMFILE_SD_H__
#define __EMFILE_SD_H__


#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include <stdbool.h>


/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/**
 * @brief Initialize emFile file system.
 *
 * This function initializes the emFile library internals including memory
 * assignment and internal data structures. Must be called before any other
 * file system operations.
 *
 * @return true  if initialization was successful
 *         false if initialization failed
 */
bool app_emfile_init(void);


/**
 * @brief Mount the SD card volume.
 *
 * Mounts the SD card volume to the file system. Assumes emFile has been
 * initialized via app_emfile_init() and the SD card device driver has been
 * added to the system via FS_X_AddDevices().
 *
 * @return true  if mount was successful
 *         false if mount failed (e.g., device not ready, invalid file system)
 */
bool app_emfile_mount(void);


/**
 * @brief Generate next filename for WAV recording.
 *
 * Generates sequential filenames in the format "rec_XXXX.wav" where XXXX is
 * a zero-padded 4-digit counter. Returns a pointer to a static internal buffer.
 *
 * @note The returned pointer is valid until the next call to this function.
 *
 * @return Pointer to filename string (e.g., "rec_0001.wav")
 */
const char* app_emfile_next_filename(void);


/**
 * @brief Find an available filename with automatic retry.
 *
 * Attempts to find an available filename by incrementing the counter if
 * the file already exists. Retries up to 10 times.
 *
 * @param[out] out_filename  Buffer to store the found filename
 * @param[in]  buffer_size   Size of the output buffer (should be >= 32 bytes)
 *
 * @return true  if an available filename was found
 *         false if all retry attempts failed
 */
bool app_emfile_find_available_filename(char* out_filename, uint32_t buffer_size);


/**
 * @brief Save PCM audio buffer as WAV file to SD card.
 *
 * Converts a PCM audio buffer to WAV format and writes it to the specified
 * file on the mounted SD card. The function creates a complete WAV file with
 * appropriate headers and audio data.
 *
 * @param[in] pcm_interleaved        Pointer to PCM audio samples in interleaved
 *                                   format (L-R-L-R for stereo). Must not be NULL.
 *
 * @param[in] num_samples_per_channel Number of audio samples per channel.
 *                                   For stereo, total samples in buffer =
 *                                   num_samples_per_channel * num_channels
 *
 * @param[in] sample_rate_hz         Sample rate in Hz (e.g., 16000 for 16 kHz)
 *
 * @param[in] num_channels           Number of audio channels (typically 1 or 2)
 *
 * @param[in] bits_per_sample        Bits per sample (typically 16)
 *
 * @param[in] filename               Pointer to filename string (e.g., "record.wav").
 *                                   Must not be NULL. File is created in the
 *                                   root of the mounted SD card.
 *
 * @return true  if WAV file was written successfully
 *         false if write failed (e.g., file creation error, disk full, write error)
 */
bool app_wav_save_from_buffer(const int16_t* pcm_interleaved,
                              uint32_t num_samples_per_channel,
                              uint32_t sample_rate_hz,
                              uint16_t num_channels,
                              uint16_t bits_per_sample,
                              const char* filename);


#if defined(__cplusplus)
}
#endif /* __cplusplus */


#endif /* __EMFILE_SD_H__ */
/* [] END OF FILE */
