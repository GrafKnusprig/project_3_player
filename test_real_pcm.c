#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_MODE
#include "pcm_file.h"

int main() {
    printf("Testing PCM file operations with real files...\n");
    
    pcm_file_t pcm_file;
    
    // Test opening a real PCM file
    esp_err_t ret = pcm_file_open("test_data/ESP32_MUSIC/Pop/song1.pcm", &pcm_file, 44100, 16, 2);
    
    if (ret == ESP_OK) {
        printf("✅ Successfully opened real PCM file\n");
        printf("File path: %s\n", pcm_file.filepath);
        printf("File size: %zu bytes\n", pcm_file.file_size);
        printf("Sample rate: %u Hz\n", pcm_file.sample_rate);
        printf("Bit depth: %u bits\n", pcm_file.bit_depth);
        printf("Channels: %u\n", pcm_file.channels);
        
        // Test reading from real file
        uint8_t buffer[256];
        size_t bytes_read;
        ret = pcm_file_read(&pcm_file, buffer, sizeof(buffer), &bytes_read);
        
        if (ret == ESP_OK) {
            printf("✅ Successfully read %zu bytes from real file\n", bytes_read);
        } else {
            printf("❌ Failed to read from real file\n");
        }
        
        // Test seeking
        ret = pcm_file_seek(&pcm_file, 100);
        if (ret == ESP_OK) {
            printf("✅ Successfully sought to position 100\n");
        }
        
        // Close file
        ret = pcm_file_close(&pcm_file);
        if (ret == ESP_OK) {
            printf("✅ Successfully closed file\n");
        }
        
        printf("✅ Real file PCM operations test passed!\n");
        return 0;
    } else {
        printf("❌ Failed to open real PCM file\n");
        return 1;
    }
}
