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
#define ESP_ERR_NO_MEM -3
#define ESP_LOGI(tag, format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[WARN] " format "\n", ##__VA_ARGS__)
typedef int esp_err_t;

// Mock SD card functions
const char* sd_card_get_mount_point() { return "/test"; }
#endif

#include "json_parser.h"

// Create test index.json file with new format
void create_test_index_json(const char* filename) {
    FILE* file = fopen(filename, "w");
    assert(file != NULL);
    
    const char* json_content = 
    "{\n"
    "  \"version\": \"1.1\",\n"
    "  \"totalFiles\": 3,\n"
    "  \"allFiles\": [\n"
    "    {\n"
    "      \"name\": \"song1.pcm\",\n"
    "      \"path\": \"Pop/song1.pcm\",\n"
    "      \"sampleRate\": 44100,\n"
    "      \"bitDepth\": 16,\n"
    "      \"channels\": 2,\n"
    "      \"folderIndex\": 0,\n"
    "      \"song\": \"Song One\",\n"
    "      \"album\": \"Pop Hits\",\n"
    "      \"artist\": \"Artist A\"\n"
    "    },\n"
    "    {\n"
    "      \"name\": \"song2.pcm\",\n"
    "      \"path\": \"Pop/song2.pcm\",\n"
    "      \"sampleRate\": 48000,\n"
    "      \"bitDepth\": 24,\n"
    "      \"channels\": 2,\n"
    "      \"folderIndex\": 0,\n"
    "      \"song\": \"Song Two\",\n"
    "      \"album\": \"Pop Hits\",\n"
    "      \"artist\": \"Artist B\"\n"
    "    },\n"
    "    {\n"
    "      \"name\": \"song3.pcm\",\n"
    "      \"path\": \"Rock/song3.pcm\",\n"
    "      \"sampleRate\": 44100,\n"
    "      \"bitDepth\": 16,\n"
    "      \"channels\": 2,\n"
    "      \"folderIndex\": 1,\n"
    "      \"song\": \"Rock Anthem\",\n"
    "      \"album\": \"Rock Collection\",\n"
    "      \"artist\": \"Artist C\"\n"
    "    }\n"
    "  ],\n"
    "  \"musicFolders\": [\n"
    "    {\n"
    "      \"name\": \"Pop\",\n"
    "      \"fileCount\": 2,\n"
    "      \"firstFileIndex\": 0\n"
    "    },\n"
    "    {\n"
    "      \"name\": \"Rock\",\n"
    "      \"fileCount\": 1,\n"
    "      \"firstFileIndex\": 2\n"
    "    }\n"
    "  ]\n"
    "}";
    
    fprintf(file, "%s", json_content);
    fclose(file);
}

void test_json_parse_index() {
    printf("Testing json_parse_index...\n");
    
    const char* test_file = "test_index.json";
    create_test_index_json(test_file);
    
    index_file_t index;
    esp_err_t ret = json_parse_index(test_file, &index);
    
    assert(ret == ESP_OK);
    assert(index.total_files == 3);
    assert(index.folder_count == 2);
    assert(index.file_positions != NULL);
    assert(index.folder_positions != NULL);
    
    // Test on-demand file reading
    file_entry_t file_entry;
    
    // Test first file
    ret = json_get_file_entry(&index, 0, &file_entry);
    assert(ret == ESP_OK);
    assert(strcmp(file_entry.name, "song1.pcm") == 0);
    assert(strcmp(file_entry.path, "Pop/song1.pcm") == 0);
    assert(file_entry.sample_rate == 44100);
    assert(file_entry.bit_depth == 16);
    assert(file_entry.channels == 2);
    assert(file_entry.folder_index == 0);
    assert(strcmp(file_entry.song, "Song One") == 0);
    assert(strcmp(file_entry.album, "Pop Hits") == 0);
    assert(strcmp(file_entry.artist, "Artist A") == 0);
    
    // Test second file with different audio format
    ret = json_get_file_entry(&index, 1, &file_entry);
    assert(ret == ESP_OK);
    assert(strcmp(file_entry.name, "song2.pcm") == 0);
    assert(file_entry.sample_rate == 48000);
    assert(file_entry.bit_depth == 24);
    assert(file_entry.folder_index == 0);
    
    // Test third file in different folder
    ret = json_get_file_entry(&index, 2, &file_entry);
    assert(ret == ESP_OK);
    assert(strcmp(file_entry.name, "song3.pcm") == 0);
    assert(file_entry.folder_index == 1);
    assert(strcmp(file_entry.song, "Rock Anthem") == 0);
    
    // Test folder reading
    folder_t folder;
    
    // Test first folder
    ret = json_get_folder_entry(&index, 0, &folder);
    assert(ret == ESP_OK);
    assert(strcmp(folder.name, "Pop") == 0);
    assert(folder.file_count == 2);
    assert(folder.first_file_index == 0);
    
    // Test second folder
    ret = json_get_folder_entry(&index, 1, &folder);
    assert(ret == ESP_OK);
    assert(strcmp(folder.name, "Rock") == 0);
    assert(folder.file_count == 1);
    assert(folder.first_file_index == 2);
    
    // Test invalid indices
    ret = json_get_file_entry(&index, -1, &file_entry);
    assert(ret != ESP_OK);
    ret = json_get_file_entry(&index, index.total_files, &file_entry);
    assert(ret != ESP_OK);
    
    ret = json_get_folder_entry(&index, -1, &folder);
    assert(ret != ESP_OK);
    ret = json_get_folder_entry(&index, index.folder_count, &folder);
    assert(ret != ESP_OK);
    
    json_free_index(&index);
    unlink(test_file);
    printf("✓ json_parse_index test passed\n");
}

void test_json_get_full_path() {
    printf("Testing json_get_full_path...\n");
    
    char full_path[256];
    esp_err_t ret = json_get_full_path("Pop/song1.pcm", full_path, sizeof(full_path));
    
    assert(ret == ESP_OK);
    assert(strcmp(full_path, "/test/ESP32_MUSIC/Pop/song1.pcm") == 0);
    
    printf("✓ json_get_full_path test passed\n");
}

void test_json_invalid_args() {
    printf("Testing json parser invalid arguments...\n");
    
    index_file_t index;
    char path[256];
    
    // Test NULL arguments
    assert(json_parse_index(NULL, &index) == ESP_ERR_INVALID_ARG);
    assert(json_parse_index("test.json", NULL) == ESP_ERR_INVALID_ARG);
    assert(json_get_full_path(NULL, path, sizeof(path)) == ESP_ERR_INVALID_ARG);
    assert(json_get_full_path("test", NULL, sizeof(path)) == ESP_ERR_INVALID_ARG);
    
    printf("✓ json parser invalid arguments test passed\n");
}

void test_json_free_index() {
    printf("Testing json_free_index...\n");
    
    const char* test_file = "test_index.json";
    create_test_index_json(test_file);
    
    index_file_t index;
    esp_err_t ret = json_parse_index(test_file, &index);
    assert(ret == ESP_OK);
    
    // Free the index
    ret = json_free_index(&index);
    assert(ret == ESP_OK);
    
    // Check that pointers are set to NULL
    assert(index.file_positions == NULL);
    assert(index.folder_positions == NULL);
    
    unlink(test_file);
    printf("✓ json_free_index test passed\n");
}

int main() {
    printf("Running JSON parser unit tests...\n\n");
    
    test_json_parse_index();
    test_json_get_full_path();
    test_json_invalid_args();
    test_json_free_index();
    
    printf("\n✅ All JSON parser tests passed!\n");
    return 0;
}
