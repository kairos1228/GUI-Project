/******************************************************************************
* File Name: wav_file.c
*
* Description: WAV file header generation implementation
*
*******************************************************************************/

#include "wav_file.h"
#include "FS.h"
#include <string.h>

/*******************************************************************************
* Function Name: wav_header_init
********************************************************************************
* Summary:
*  Initialize a WAV file header for 16kHz, 16-bit, stereo PCM audio
*
* Parameters:
*  header: Pointer to wav_header_t structure to initialize
*  total_samples: Total number of samples per channel (not total sample count)
*                 Example: 4 seconds at 16kHz = 64000 samples/channel
*
* Return:
*  None
*
*******************************************************************************/
void wav_header_init(wav_header_t *header, uint32_t total_samples)
{
    uint32_t data_bytes;
    uint32_t byte_rate;
    
    /* Calculate sizes */
    data_bytes = total_samples * WAV_NUM_CHANNELS * (WAV_BITS_PER_SAMPLE / 8);
    byte_rate = WAV_SAMPLE_RATE * WAV_NUM_CHANNELS * (WAV_BITS_PER_SAMPLE / 8);
    
    /* RIFF Chunk Descriptor */
    header->riff_header[0] = 'R';
    header->riff_header[1] = 'I';
    header->riff_header[2] = 'F';
    header->riff_header[3] = 'F';
    header->wav_size = 36 + data_bytes;  /* Total file size - 8 bytes */
    header->wave_header[0] = 'W';
    header->wave_header[1] = 'A';
    header->wave_header[2] = 'V';
    header->wave_header[3] = 'E';
    
    /* fmt sub-chunk */
    header->fmt_header[0] = 'f';
    header->fmt_header[1] = 'm';
    header->fmt_header[2] = 't';
    header->fmt_header[3] = ' ';
    header->fmt_chunk_size = 16;                    /* PCM format */
    header->audio_format = 1;                       /* PCM = 1 */
    header->num_channels = WAV_NUM_CHANNELS;        /* Stereo = 2 */
    header->sample_rate = WAV_SAMPLE_RATE;          /* 16000 Hz */
    header->byte_rate = byte_rate;                  /* 64000 bytes/sec */
    header->block_align = WAV_NUM_CHANNELS * (WAV_BITS_PER_SAMPLE / 8);  /* 4 bytes */
    header->bits_per_sample = WAV_BITS_PER_SAMPLE;  /* 16 bits */
    
    /* data sub-chunk */
    header->data_header[0] = 'd';
    header->data_header[1] = 'a';
    header->data_header[2] = 't';
    header->data_header[3] = 'a';
    header->data_bytes = data_bytes;
}

/*******************************************************************************
* Function Name: wav_file_save
********************************************************************************
* Summary:
*  Save WAV file to SD card using emFile API
*
* Parameters:
*  filename: Path to the WAV file (e.g., "audio_001.wav")
*  wav_header: Pointer to initialized WAV header structure
*  pcm_buffer: Pointer to PCM audio data buffer (int16_t array)
*  num_samples: Number of samples per channel to write
*
* Return:
*  0 on success, -1 on error
*
*******************************************************************************/
int wav_file_save(const char *filename, 
                  const wav_header_t *wav_header, 
                  const int16_t *pcm_buffer, 
                  uint32_t num_samples)
{
    FS_FILE *file;
    uint32_t bytes_to_write;
    uint32_t bytes_written;
    
    /* Open file for writing (create if not exists, overwrite if exists) */
    file = FS_FOpen(filename, "w");
    if (file == NULL) {
        return -1;  /* File open error */
    }
    
    /* Write WAV header (44 bytes) */
    bytes_written = FS_Write(file, wav_header, WAV_HEADER_SIZE);
    if (bytes_written != WAV_HEADER_SIZE) {
        FS_FClose(file);
        return -1;  /* Header write error */
    }
    
    /* Write PCM data */
    bytes_to_write = num_samples * WAV_NUM_CHANNELS * (WAV_BITS_PER_SAMPLE / 8);
    bytes_written = FS_Write(file, pcm_buffer, bytes_to_write);
    if (bytes_written != bytes_to_write) {
        FS_FClose(file);
        return -1;  /* PCM data write error */
    }
    
    /* Close file */
    if (FS_FClose(file) != 0) {
        return -1;  /* File close error */
    }
    
    return 0;  /* Success */
}
