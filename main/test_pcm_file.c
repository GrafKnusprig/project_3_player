#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// Test mode definitions to avoid ESP-IDF dependencies
#ifdef TEST_MODE
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_LOGI(tag, format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
typedef int esp_err_t;
#endif

#include "pcm_file.h"

// Create a test PCM file
void create_test_pcm_file(const char* filename, size_t size) {
    FILE* file = fopen(filename, "wb");
    assert(file != NULL);
    
    // Write some test data
    for (size_t i = 0; i < size; i++) {
        uint8_t data = (uint8_t)(i % 256);
        fwrite(&data, 1, 1, file);
    }
    
    fclose(file);
}

void test_pcm_file_open() {
    printf("Testing pcm_file_open...\n");
    
    const char* test_file = "test_audio.pcm";
    create_test_pcm_file(test_file, 1024);
    
    pcm_file_t pcm_file;
    esp_err_t ret = pcm_file_open(test_file, &pcm_file, 44100, 16, 2);
    
    assert(ret == ESP_OK);
    assert(pcm_file.file != NULL);
    assert(strcmp(pcm_file.filepath, test_file) == 0);
    assert(pcm_file.sample_rate == 44100);
    assert(pcm_file.bit_depth == 16);
    assert(pcm_file.channels == 2);
    assert(pcm_file.file_size == 1024);
    assert(pcm_file.position == 0);
    
    pcm_file_close(&pcm_file);
    unlink(test_file);
    printf("✓ pcm_file_open test passed\n");
}

void test_pcm_file_read() {
    printf("Testing pcm_file_read...\n");
    
    const char* test_file = "test_audio.pcm";
    create_test_pcm_file(test_file, 100);
    
    pcm_file_t pcm_file;
    esp_err_t ret = pcm_file_open(test_file, &pcm_file, 22050, 8, 1);
    assert(ret == ESP_OK);
    
    uint8_t buffer[50];
    size_t bytes_read;
    ret = pcm_file_read(&pcm_file, buffer, sizeof(buffer), &bytes_read);
    
    assert(ret == ESP_OK);
    assert(bytes_read == 50);
    assert(pcm_file.position == 50);
    
    // Verify data
    for (int i = 0; i < 50; i++) {
        assert(buffer[i] == (uint8_t)(i % 256));
    }
    
    pcm_file_close(&pcm_file);
    unlink(test_file);
    printf("✓ pcm_file_read test passed\n");
}

void test_pcm_file_seek() {
    printf("Testing pcm_file_seek...\n");
    
    const char* test_file = "test_audio.pcm";
    create_test_pcm_file(test_file, 100);
    
    pcm_file_t pcm_file;
    esp_err_t ret = pcm_file_open(test_file, &pcm_file, 48000, 24, 2);
    assert(ret == ESP_OK);
    
    // Seek to position 50
    ret = pcm_file_seek(&pcm_file, 50);
    assert(ret == ESP_OK);
    assert(pcm_file.position == 50);
    
    // Read and verify we're at the right position
    uint8_t data;
    size_t bytes_read;
    ret = pcm_file_read(&pcm_file, &data, 1, &bytes_read);
    assert(ret == ESP_OK);
    assert(bytes_read == 1);
    assert(data == 50); // Should be the 50th byte
    assert(pcm_file.position == 51);
    
    pcm_file_close(&pcm_file);
    unlink(test_file);
    printf("✓ pcm_file_seek test passed\n");
}

void test_pcm_file_get_params() {
    printf("Testing pcm_file_get_params...\n");
    
    const char* test_file = "test_audio.pcm";
    create_test_pcm_file(test_file, 50);
    
    pcm_file_t pcm_file;
    esp_err_t ret = pcm_file_open(test_file, &pcm_file, 96000, 32, 4);
    assert(ret == ESP_OK);
    
    uint32_t sample_rate;
    uint16_t bit_depth;
    uint16_t channels;
    
    ret = pcm_file_get_params(&pcm_file, &sample_rate, &bit_depth, &channels);
    assert(ret == ESP_OK);
    assert(sample_rate == 96000);
    assert(bit_depth == 32);
    assert(channels == 4);
    
    pcm_file_close(&pcm_file);
    unlink(test_file);
    printf("✓ pcm_file_get_params test passed\n");
}

void test_pcm_file_invalid_args() {
    printf("Testing pcm_file invalid arguments...\n");
    
    pcm_file_t pcm_file;
    uint8_t buffer[10];
    size_t bytes_read;
    uint32_t sample_rate;
    uint16_t bit_depth;
    uint16_t channels;
    
    // Test NULL arguments
    assert(pcm_file_open(NULL, &pcm_file, 44100, 16, 2) == ESP_ERR_INVALID_ARG);
    assert(pcm_file_open("test.pcm", NULL, 44100, 16, 2) == ESP_ERR_INVALID_ARG);
    assert(pcm_file_read(NULL, buffer, 10, &bytes_read) == ESP_ERR_INVALID_ARG);
    assert(pcm_file_get_params(NULL, &sample_rate, &bit_depth, &channels) == ESP_ERR_INVALID_ARG);
    assert(pcm_file_get_params(&pcm_file, NULL, &bit_depth, &channels) == ESP_ERR_INVALID_ARG);
    
    printf("✓ pcm_file invalid arguments test passed\n");
}

int main() {
    printf("Running PCM file unit tests...\n\n");
    
    test_pcm_file_open();
    test_pcm_file_read();
    test_pcm_file_seek();
    test_pcm_file_get_params();
    test_pcm_file_invalid_args();
    
    printf("\n✅ All PCM file tests passed!\n");
    return 0;
}
