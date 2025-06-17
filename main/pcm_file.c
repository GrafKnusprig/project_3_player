#include "pcm_file.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "esp_log.h"
#include "sd_card.h" // Add for path resolution

static const char *TAG = "pcm_file";

esp_err_t pcm_file_open(const char *filepath, pcm_file_t *pcm_file) {
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

    // Read the header
    size_t read_size = fread(&pcm_file->header, 1, sizeof(pcm_header_t), pcm_file->file);
    if (read_size != sizeof(pcm_header_t)) {
        ESP_LOGE(TAG, "Failed to read PCM header");
        fclose(pcm_file->file);
        return ESP_FAIL;
    }

    // Verify the magic number
    if (strncmp(pcm_file->header.magic, PCM_MAGIC, strlen(PCM_MAGIC)) != 0) {
        ESP_LOGE(TAG, "Invalid PCM file format, wrong magic number");
        fclose(pcm_file->file);
        return ESP_FAIL;
    }

    // Store the filepath
    strncpy(pcm_file->filepath, filepath, sizeof(pcm_file->filepath) - 1);
    pcm_file->filepath[sizeof(pcm_file->filepath) - 1] = '\0';
    
    // Initialize position
    pcm_file->position = 0;
    
    ESP_LOGI(TAG, "PCM file opened: %s", filepath);
    ESP_LOGI(TAG, "Sample rate: %lu Hz, Bit depth: %u bits, Channels: %u", 
             pcm_file->header.sample_rate, pcm_file->header.bit_depth, pcm_file->header.channels);
    
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

pcm_header_t pcm_file_get_header(pcm_file_t *pcm_file) {
    return pcm_file->header;
}
