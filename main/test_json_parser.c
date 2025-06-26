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
    "      \"files\": [\n"
    "        {\n"
    "          \"name\": \"song1.pcm\",\n"
    "          \"path\": \"Pop/song1.pcm\",\n"
    "          \"sampleRate\": 44100,\n"
    "          \"bitDepth\": 16,\n"
    "          \"channels\": 2,\n"
    "          \"song\": \"Song One\",\n"
    "          \"album\": \"Pop Hits\",\n"
    "          \"artist\": \"Artist A\"\n"
    "        },\n"
    "        {\n"
    "          \"name\": \"song2.pcm\",\n"
    "          \"path\": \"Pop/song2.pcm\",\n"
    "          \"sampleRate\": 48000,\n"
    "          \"bitDepth\": 24,\n"
    "          \"channels\": 2,\n"
    "          \"song\": \"Song Two\",\n"
    "          \"album\": \"Pop Hits\",\n"
    "          \"artist\": \"Artist B\"\n"
    "        }\n"
    "      ]\n"
    "    },\n"
    "    {\n"
    "      \"name\": \"Rock\",\n"
    "      \"files\": [\n"
    "        {\n"
    "          \"name\": \"song3.pcm\",\n"
    "          \"path\": \"Rock/song3.pcm\",\n"
    "          \"sampleRate\": 44100,\n"
    "          \"bitDepth\": 16,\n"
    "          \"channels\": 2,\n"
    "          \"song\": \"Rock Anthem\",\n"
    "          \"album\": \"Rock Collection\",\n"
    "          \"artist\": \"Artist C\"\n"
    "        }\n"
    "      ]\n"
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
    assert(strcmp(index.version, "1.1") == 0);
    assert(index.total_files == 3);
    assert(index.folder_count == 2);
    
    // Test allFiles array
    assert(index.all_files != NULL);
    assert(strcmp(index.all_files[0].name, "song1.pcm") == 0);
    assert(strcmp(index.all_files[0].path, "Pop/song1.pcm") == 0);
    assert(index.all_files[0].sample_rate == 44100);
    assert(index.all_files[0].bit_depth == 16);
    assert(index.all_files[0].channels == 2);
    assert(index.all_files[0].folder_index == 0);
    assert(strcmp(index.all_files[0].song, "Song One") == 0);
    assert(strcmp(index.all_files[0].album, "Pop Hits") == 0);
    assert(strcmp(index.all_files[0].artist, "Artist A") == 0);
    
    // Test second file with different audio format
    assert(strcmp(index.all_files[1].name, "song2.pcm") == 0);
    assert(index.all_files[1].sample_rate == 48000);
    assert(index.all_files[1].bit_depth == 24);
    assert(index.all_files[1].folder_index == 0);
    
    // Test third file in different folder
    assert(strcmp(index.all_files[2].name, "song3.pcm") == 0);
    assert(index.all_files[2].folder_index == 1);
    assert(strcmp(index.all_files[2].song, "Rock Anthem") == 0);
    
    // Test musicFolders array
    assert(index.music_folders != NULL);
    assert(strcmp(index.music_folders[0].name, "Pop") == 0);
    assert(index.music_folders[0].file_count == 2);
    if (index.folder_count > 1) {
        assert(strcmp(index.music_folders[1].name, "Rock") == 0);
        assert(index.music_folders[1].file_count == 1);
    }
    
    // Test folder files
    assert(index.music_folders[0].files != NULL);
    assert(strcmp(index.music_folders[0].files[0].name, "song1.pcm") == 0);
    assert(strcmp(index.music_folders[0].files[1].name, "song2.pcm") == 0);
    
    assert(index.music_folders[1].files != NULL);
    assert(strcmp(index.music_folders[1].files[0].name, "song3.pcm") == 0);
    
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
    assert(index.all_files == NULL);
    assert(index.music_folders == NULL);
    
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
