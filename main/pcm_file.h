#ifndef PCM_FILE_H
#define PCM_FILE_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#ifndef TEST_MODE
#include "esp_err.h"
#else
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#endif

// PCM file handle - no more custom headers, files are plain PCM
typedef struct {
    FILE *file;
    char filepath[256];
    uint32_t position;
    uint32_t sample_rate;   // Sample rate (e.g., 44100 Hz)
    uint16_t bit_depth;     // Bit depth (e.g., 16 bits)
    uint16_t channels;      // Number of channels (e.g., 2 for stereo)
    size_t file_size;       // Total file size in bytes
} pcm_file_t;

/**
 * @brief Open a PCM file
 * 
 * @param filepath Path to the PCM file
 * @param pcm_file Pointer to store PCM file handle
 * @param sample_rate Sample rate for this file
 * @param bit_depth Bit depth for this file
 * @param channels Number of channels for this file
 * @return ESP_OK on success
 */
esp_err_t pcm_file_open(const char *filepath, pcm_file_t *pcm_file, uint32_t sample_rate, uint16_t bit_depth, uint16_t channels);

/**
 * @brief Close a PCM file
 * 
 * @param pcm_file PCM file handle
 * @return ESP_OK on success
 */
esp_err_t pcm_file_close(pcm_file_t *pcm_file);

/**
 * @brief Read PCM data from file
 * 
 * @param pcm_file PCM file handle
 * @param buffer Buffer to store read data
 * @param buffer_size Size of buffer
 * @param bytes_read Pointer to store number of bytes read
 * @return ESP_OK on success
 */
esp_err_t pcm_file_read(pcm_file_t *pcm_file, void *buffer, size_t buffer_size, size_t *bytes_read);

/**
 * @brief Get PCM file audio parameters
 * 
 * @param pcm_file PCM file handle
 * @param sample_rate Pointer to store sample rate
 * @param bit_depth Pointer to store bit depth
 * @param channels Pointer to store number of channels
 * @return ESP_OK on success
 */
esp_err_t pcm_file_get_params(pcm_file_t *pcm_file, uint32_t *sample_rate, uint16_t *bit_depth, uint16_t *channels);

/**
 * @brief Seek to a specific byte position in the PCM data
 * 
 * @param pcm_file PCM file handle
 * @param byte_pos Byte position from start of file
 * @return ESP_OK on success
 */
esp_err_t pcm_file_seek(pcm_file_t *pcm_file, uint32_t byte_pos);

#endif // PCM_FILE_H
