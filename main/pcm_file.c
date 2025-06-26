#include "pcm_file.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifndef TEST_MODE
#include "esp_log.h"
#include "sd_card.h" // Add for path resolution
#else
// Test mode definitions
#define ESP_LOGI(tag, format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#endif

static const char *TAG = "pcm_file";

esp_err_t pcm_file_open(const char *filepath, pcm_file_t *pcm_file, uint32_t sample_rate, uint16_t bit_depth, uint16_t channels) {
    if (filepath == NULL || pcm_file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Opening PCM file: %s", filepath);
    
    // Open the file directly
    pcm_file->file = fopen(filepath, "rb");
    
    // Check if we have an open file
    if (pcm_file->file == NULL) {
        ESP_LOGE(TAG, "Failed to open PCM file: %s (errno: %d)", filepath, errno);
        return ESP_FAIL;
    }

    // Store the filepath and audio parameters
    strncpy(pcm_file->filepath, filepath, sizeof(pcm_file->filepath) - 1);
    pcm_file->filepath[sizeof(pcm_file->filepath) - 1] = '\0';
    
    // Set audio parameters from metadata
    pcm_file->sample_rate = sample_rate;
    pcm_file->bit_depth = bit_depth;
    pcm_file->channels = channels;
    
    // Get file size
    fseek(pcm_file->file, 0, SEEK_END);
    pcm_file->file_size = ftell(pcm_file->file);
    fseek(pcm_file->file, 0, SEEK_SET);
    
    // Initialize position
    pcm_file->position = 0;
    
    ESP_LOGI(TAG, "PCM file opened: %s", filepath);
    ESP_LOGI(TAG, "Sample rate: %u Hz, Bit depth: %u bits, Channels: %u, Size: %zu bytes", 
             pcm_file->sample_rate, pcm_file->bit_depth, pcm_file->channels, pcm_file->file_size);
    
    return ESP_OK;
}

esp_err_t pcm_file_seek(pcm_file_t *pcm_file, uint32_t byte_pos) {
    if (pcm_file == NULL || pcm_file->file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (fseek(pcm_file->file, byte_pos, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek to byte position %u in PCM file (errno: %d)", byte_pos, errno);
        return ESP_FAIL;
    }

    pcm_file->position = byte_pos;

    ESP_LOGI(TAG, "PCM file seek: byte_pos=%u, new position=%u", byte_pos, pcm_file->position);

    return ESP_OK;
}

esp_err_t pcm_file_close(pcm_file_t *pcm_file) {
    if (pcm_file == NULL || pcm_file->file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    fclose(pcm_file->file);
    pcm_file->file = NULL;
    ESP_LOGI(TAG, "PCM file closed");
    
    return ESP_OK;
}

esp_err_t pcm_file_read(pcm_file_t *pcm_file, void *buffer, size_t buffer_size, size_t *bytes_read) {
    if (pcm_file == NULL || pcm_file->file == NULL || buffer == NULL || bytes_read == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Read data from the file
    *bytes_read = fread(buffer, 1, buffer_size, pcm_file->file);
    
    // Update position
    pcm_file->position += *bytes_read;
    
    // Check if we've reached the end of file
    if (*bytes_read < buffer_size) {
        if (feof(pcm_file->file)) {
            ESP_LOGI(TAG, "End of PCM file reached");
        } else {
            ESP_LOGE(TAG, "Error reading PCM file");
            return ESP_FAIL;
        }
    }
    
    return ESP_OK;
}

esp_err_t pcm_file_get_params(pcm_file_t *pcm_file, uint32_t *sample_rate, uint16_t *bit_depth, uint16_t *channels) {
    if (pcm_file == NULL || sample_rate == NULL || bit_depth == NULL || channels == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *sample_rate = pcm_file->sample_rate;
    *bit_depth = pcm_file->bit_depth;
    *channels = pcm_file->channels;
    
    return ESP_OK;
}
