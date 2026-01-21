/******************************************************************************
* File Name: wav_file.h
*
* Description: WAV file header generation utilities
*
*******************************************************************************/

#ifndef __WAV_FILE_H__
#define __WAV_FILE_H__

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*******************************************************************************
* Macros
*******************************************************************************/
#define WAV_HEADER_SIZE             (44u)
#define WAV_SAMPLE_RATE             (16000u)
#define WAV_BITS_PER_SAMPLE         (16u)
#define WAV_NUM_CHANNELS            (2u)

/*******************************************************************************
* Structures
*******************************************************************************/
/* Standard RIFF/WAVE header structure (44 bytes) */
typedef struct __attribute__((packed)) {
    /* RIFF Chunk Descriptor */
    uint8_t  riff_header[4];        /* "RIFF" */
    uint32_t wav_size;              /* File size - 8 */
    uint8_t  wave_header[4];        /* "WAVE" */
    
    /* fmt sub-chunk */
    uint8_t  fmt_header[4];         /* "fmt " */
    uint32_t fmt_chunk_size;        /* 16 for PCM */
    uint16_t audio_format;          /* 1 for PCM */
    uint16_t num_channels;          /* 2 for stereo */
    uint32_t sample_rate;           /* 16000 */
    uint32_t byte_rate;             /* sample_rate * num_channels * bits_per_sample/8 */
    uint16_t block_align;           /* num_channels * bits_per_sample/8 */
    uint16_t bits_per_sample;       /* 16 */
    
    /* data sub-chunk */
    uint8_t  data_header[4];        /* "data" */
    uint32_t data_bytes;            /* num_samples * num_channels * bits_per_sample/8 */
} wav_header_t;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void wav_header_init(wav_header_t *header, uint32_t total_samples);
int wav_file_save(const char *filename, 
                  const wav_header_t *wav_header, 
                  const int16_t *pcm_buffer, 
                  uint32_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* __WAV_FILE_H__ */
