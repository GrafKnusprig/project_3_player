#ifndef PCM_FILE_H
#define PCM_FILE_H

#include <stdint.h>
#include "esp_err.h"

#define PCM_HEADER_SIZE 32
#define PCM_MAGIC "ESP32PCM"

// PCM file header
typedef struct {
    char magic[8];          // Magic number "ESP32PCM"
    uint32_t sample_rate;   // Sample rate (e.g., 44100 Hz)
    uint16_t bit_depth;     // Bit depth (e.g., 16 bits)
    uint16_t channels;      // Number of channels (e.g., 2 for stereo)
    uint32_t data_size;     // Data size in bytes
    uint8_t reserved[12];   // Reserved (padding, filled with zeros)
} __attribute__((packed)) pcm_header_t;

// PCM file handle
typedef struct {
    pcm_header_t header;
    FILE *file;
    char filepath[256];
    uint32_t position;
} pcm_file_t;

/**
 * @brief Open a PCM file
 * 
 * @param filepath Path to the PCM file
 * @param pcm_file Pointer to store PCM file handle
 * @return ESP_OK on success
 */
esp_err_t pcm_file_open(const char *filepath, pcm_file_t *pcm_file);

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
 * @brief Get PCM file header
 * 
 * @param pcm_file PCM file handle
 * @return PCM header
 */
pcm_header_t pcm_file_get_header(pcm_file_t *pcm_file);

/**
 * @brief Seek to a specific byte position in the PCM data section
 * 
 * @param pcm_file PCM file handle
 * @param byte_pos Byte position relative to start of PCM data (not header)
 * @return ESP_OK on success
 */
esp_err_t pcm_file_seek(pcm_file_t *pcm_file, uint32_t byte_pos);

#endif // PCM_FILE_H
