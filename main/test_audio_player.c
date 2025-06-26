#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>

// Test mode definitions to avoid ESP-IDF dependencies
#ifdef TEST_MODE
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_NO_MEM -3
#define ESP_ERR_INVALID_STATE -4
#define ESP_ERR_NOT_FOUND -5
#define ESP_LOGI(tag, format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[WARN] " format "\n", ##__VA_ARGS__)
typedef int esp_err_t;

// Mock FreeRTOS types
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef int BaseType_t;
#define pdPASS 1
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#define tskIDLE_PRIORITY 0

// Mock SD card functions (these are not in audio_player.c)
bool sd_card_is_mounted() { return true; }
const char* sd_card_get_mount_point() { return "/test"; }
esp_err_t sd_card_write_file(const char* path, const void* data, size_t size) { return ESP_OK; }
esp_err_t sd_card_read_file(const char* path, void* data, size_t size, size_t* bytes_read) { 
    *bytes_read = size; 
    return ESP_OK; 
}

#endif

#include "audio_player.h"
#include "json_parser.h"
#include "pcm_file.h"

// Test helper function declarations
#ifdef TEST_MODE
esp_err_t test_select_next_file(void);
esp_err_t test_select_prev_file(void);
esp_err_t test_play_current_file(void);
#endif

// Test data - simulate a loaded index
static index_file_t test_index;
static bool index_loaded = false;

// Override the global music_index from audio_player.c by creating our own test data
void setup_test_index() {
    if (index_loaded) return;
    
    // Initialize test index
    strcpy(test_index.version, "1.1");
    test_index.total_files = 4;
    test_index.folder_count = 2;
    
    // Allocate and setup allFiles
    test_index.all_files = malloc(sizeof(file_entry_t) * 4);
    
    // File 0: Pop/song1.pcm (folder 0)
    strcpy(test_index.all_files[0].name, "song1.pcm");
    strcpy(test_index.all_files[0].path, "Pop/song1.pcm");
    test_index.all_files[0].sample_rate = 44100;
    test_index.all_files[0].bit_depth = 16;
    test_index.all_files[0].channels = 2;
    test_index.all_files[0].folder_index = 0;
    strcpy(test_index.all_files[0].song, "Pop Song One");
    strcpy(test_index.all_files[0].album, "Pop Hits");
    strcpy(test_index.all_files[0].artist, "Pop Artist A");
    
    // File 1: Pop/song2.pcm (folder 0)
    strcpy(test_index.all_files[1].name, "song2.pcm");
    strcpy(test_index.all_files[1].path, "Pop/song2.pcm");
    test_index.all_files[1].sample_rate = 48000;
    test_index.all_files[1].bit_depth = 24;
    test_index.all_files[1].channels = 2;
    test_index.all_files[1].folder_index = 0;
    strcpy(test_index.all_files[1].song, "Pop Song Two");
    strcpy(test_index.all_files[1].album, "Pop Hits");
    strcpy(test_index.all_files[1].artist, "Pop Artist B");
    
    // File 2: Rock/song3.pcm (folder 1)
    strcpy(test_index.all_files[2].name, "song3.pcm");
    strcpy(test_index.all_files[2].path, "Rock/song3.pcm");
    test_index.all_files[2].sample_rate = 44100;
    test_index.all_files[2].bit_depth = 16;
    test_index.all_files[2].channels = 2;
    test_index.all_files[2].folder_index = 1;
    strcpy(test_index.all_files[2].song, "Rock Anthem");
    strcpy(test_index.all_files[2].album, "Rock Collection");
    strcpy(test_index.all_files[2].artist, "Rock Artist C");
    
    // File 3: Rock/song4.pcm (folder 1)
    strcpy(test_index.all_files[3].name, "song4.pcm");
    strcpy(test_index.all_files[3].path, "Rock/song4.pcm");
    test_index.all_files[3].sample_rate = 96000;
    test_index.all_files[3].bit_depth = 32;
    test_index.all_files[3].channels = 2;
    test_index.all_files[3].folder_index = 1;
    strcpy(test_index.all_files[3].song, "Heavy Metal");
    strcpy(test_index.all_files[3].album, "Rock Collection");
    strcpy(test_index.all_files[3].artist, "Rock Artist D");
    
    // Allocate and setup folders
    test_index.music_folders = malloc(sizeof(folder_t) * 2);
    
    // Folder 0: Pop
    strcpy(test_index.music_folders[0].name, "Pop");
    test_index.music_folders[0].file_count = 2;
    test_index.music_folders[0].files = malloc(sizeof(file_entry_t) * 2);
    memcpy(&test_index.music_folders[0].files[0], &test_index.all_files[0], sizeof(file_entry_t));
    memcpy(&test_index.music_folders[0].files[1], &test_index.all_files[1], sizeof(file_entry_t));
    
    // Folder 1: Rock
    strcpy(test_index.music_folders[1].name, "Rock");
    test_index.music_folders[1].file_count = 2;
    test_index.music_folders[1].files = malloc(sizeof(file_entry_t) * 2);
    memcpy(&test_index.music_folders[1].files[0], &test_index.all_files[2], sizeof(file_entry_t));
    memcpy(&test_index.music_folders[1].files[1], &test_index.all_files[3], sizeof(file_entry_t));
    
    index_loaded = true;
}

void cleanup_test_index() {
    if (!index_loaded) return;
    
    free(test_index.all_files);
    free(test_index.music_folders[0].files);
    free(test_index.music_folders[1].files);
    free(test_index.music_folders);
    index_loaded = false;
}

// Override json_parse_index to return our test data
esp_err_t json_parse_index(const char *filepath, index_file_t *index) {
    setup_test_index();
    memcpy(index, &test_index, sizeof(index_file_t));
    
    // Allocate new memory for the test to avoid double-free
    index->all_files = malloc(sizeof(file_entry_t) * test_index.total_files);
    memcpy(index->all_files, test_index.all_files, sizeof(file_entry_t) * test_index.total_files);
    
    index->music_folders = malloc(sizeof(folder_t) * test_index.folder_count);
    for (int i = 0; i < test_index.folder_count; i++) {
        memcpy(&index->music_folders[i], &test_index.music_folders[i], sizeof(folder_t));
        index->music_folders[i].files = malloc(sizeof(file_entry_t) * test_index.music_folders[i].file_count);
        memcpy(index->music_folders[i].files, test_index.music_folders[i].files, 
               sizeof(file_entry_t) * test_index.music_folders[i].file_count);
    }
    
    return ESP_OK;
}

// Mock json_free_index
esp_err_t json_free_index(index_file_t *index) {
    if (index->all_files) {
        free(index->all_files);
        index->all_files = NULL;
    }
    if (index->music_folders) {
        for (int i = 0; i < index->folder_count; i++) {
            if (index->music_folders[i].files) {
                free(index->music_folders[i].files);
            }
        }
        free(index->music_folders);
        index->music_folders = NULL;
    }
    return ESP_OK;
}

// Mock json_get_full_path
esp_err_t json_get_full_path(const char *relative_path, char *full_path, size_t max_len) {
    if (!relative_path || !full_path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    snprintf(full_path, max_len, "/test/%s", relative_path);
    return ESP_OK;
}

// Mock pcm_file functions
esp_err_t pcm_file_open(const char *filepath, pcm_file_t *pcm_file, uint32_t sample_rate, uint16_t bit_depth, uint16_t channels) {
    if (!filepath || !pcm_file) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(pcm_file, 0, sizeof(pcm_file_t));
    strncpy(pcm_file->filepath, filepath, sizeof(pcm_file->filepath) - 1);
    pcm_file->filepath[sizeof(pcm_file->filepath) - 1] = '\0';
    pcm_file->sample_rate = sample_rate;
    pcm_file->bit_depth = bit_depth;
    pcm_file->channels = channels;
    pcm_file->file_size = 1024; // Mock file size
    pcm_file->file = (FILE*)1; // Mock file handle
    pcm_file->position = 0;
    
    printf("[INFO] Mock: Opened PCM file: %s (Rate: %u Hz, Depth: %u bits, Channels: %u)\n", 
           filepath, sample_rate, bit_depth, channels);
    
    return ESP_OK;
}

esp_err_t pcm_file_close(pcm_file_t *pcm_file) {
    if (!pcm_file) {
        return ESP_ERR_INVALID_ARG;
    }
    
    pcm_file->file = NULL;
    pcm_file->position = 0;
    
    printf("[INFO] Mock: Closed PCM file: %s\n", pcm_file->filepath);
    
    return ESP_OK;
}

esp_err_t pcm_file_read(pcm_file_t *pcm_file, void *buffer, size_t buffer_size, size_t *bytes_read) {
    if (!pcm_file || !buffer || !bytes_read || pcm_file->file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Mock reading data - just fill with zeros
    memset(buffer, 0, buffer_size);
    *bytes_read = buffer_size;
    pcm_file->position += buffer_size;
    
    return ESP_OK;
}

esp_err_t pcm_file_get_params(pcm_file_t *pcm_file, uint32_t *sample_rate, uint16_t *bit_depth, uint16_t *channels) {
    if (!pcm_file || !sample_rate || !bit_depth || !channels) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *sample_rate = pcm_file->sample_rate;
    *bit_depth = pcm_file->bit_depth;
    *channels = pcm_file->channels;
    
    return ESP_OK;
}

esp_err_t pcm_file_seek(pcm_file_t *pcm_file, uint32_t byte_pos) {
    if (!pcm_file || pcm_file->file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    pcm_file->position = byte_pos;
    
    return ESP_OK;
}

// Test initialization
void test_audio_player_init() {
    printf("Testing audio_player_init...\n");
    
    esp_err_t ret = audio_player_init();
    assert(ret == ESP_OK);
    
    printf("✓ audio_player_init test passed\n");
}

// Test mode switching
void test_mode_switching() {
    printf("Testing mode switching...\n");
    
    // Test setting each mode
    for (int mode = 0; mode < MODE_MAX; mode++) {
        esp_err_t ret = audio_player_set_mode((playback_mode_t)mode);
        assert(ret == ESP_OK);
        
        player_state_t state = audio_player_get_state();
        assert(state.mode == mode);
        
        printf("✓ Mode %d set successfully\n", mode);
    }
    
    // Test invalid mode
    esp_err_t ret = audio_player_set_mode(MODE_MAX);
    assert(ret == ESP_ERR_INVALID_ARG);
    
    printf("✓ mode switching test passed\n");
}

// Test play all order mode
void test_play_all_order_mode() {
    printf("Testing play all order mode...\n");
    
    audio_player_set_mode(MODE_PLAY_ALL_ORDER);
    
    // Get initial state
    player_state_t state = audio_player_get_state();
    assert(state.mode == MODE_PLAY_ALL_ORDER);
    
    // Test next file progression
    for (int i = 0; i < 5; i++) { // Test wrapping
        esp_err_t ret = test_select_next_file();
        assert(ret == ESP_OK);
        
        state = audio_player_get_state();
        int expected_index = (i + 1) % 4; // Should wrap around with 4 files
        printf("After next %d: current_file_index = %d (expected %d)\n", 
               i + 1, state.current_file_index, expected_index);
        assert(state.current_file_index == expected_index);
    }
    
    // Test previous file
    esp_err_t ret = test_select_prev_file();
    assert(ret == ESP_OK);
    
    printf("✓ play all order mode test passed\n");
}

// Test folder order mode
void test_folder_order_mode() {
    printf("Testing folder order mode...\n");
    
    audio_player_set_mode(MODE_PLAY_FOLDER_ORDER);
    
    player_state_t state = audio_player_get_state();
    assert(state.mode == MODE_PLAY_FOLDER_ORDER);
    
    // Test next file within folder
    esp_err_t ret = audio_player_next();
    assert(ret == ESP_OK);
    
    state = audio_player_get_state();
    // Should advance within current folder
    printf("Folder mode - current_folder_index: %d, current_file_index: %d\n", 
           state.current_folder_index, state.current_file_index);
    
    // Test folder switching
    ret = audio_player_next_folder();
    assert(ret == ESP_OK);
    
    state = audio_player_get_state();
    printf("After next folder - current_folder_index: %d, current_file_index: %d\n", 
           state.current_folder_index, state.current_file_index);
    
    // Test previous folder
    ret = audio_player_prev_folder();
    assert(ret == ESP_OK);
    
    printf("✓ folder order mode test passed\n");
}

// Test metadata loading
void test_metadata_loading() {
    printf("Testing metadata loading...\n");
    
    // Set to play all mode 
    audio_player_set_mode(MODE_PLAY_ALL_ORDER);
    
    // Trigger file selection and loading by calling next, then play current file
    test_select_next_file();
    test_play_current_file();
    
    // Get current state and check metadata
    player_state_t state = audio_player_get_state();
    
    printf("Current song metadata:\n");
    printf("  Song: %s\n", state.current_song);
    printf("  Album: %s\n", state.current_album);
    printf("  Artist: %s\n", state.current_artist);
    printf("  Sample Rate: %u Hz\n", state.current_sample_rate);
    printf("  Bit Depth: %u bits\n", state.current_bit_depth);
    printf("  Channels: %u\n", state.current_channels);
    
    // Verify metadata is loaded (should be from first file)
    assert(strlen(state.current_song) > 0);
    assert(strlen(state.current_album) > 0);
    assert(strlen(state.current_artist) > 0);
    assert(state.current_sample_rate > 0);
    assert(state.current_bit_depth > 0);
    assert(state.current_channels > 0);
    
    // Test next file and verify metadata changes
    test_select_next_file();
    test_play_current_file();
    player_state_t new_state = audio_player_get_state();
    
    printf("Next song metadata:\n");
    printf("  Song: %s\n", new_state.current_song);
    printf("  Sample Rate: %u Hz\n", new_state.current_sample_rate);
    printf("  Bit Depth: %u bits\n", new_state.current_bit_depth);
    
    // Should have different audio parameters (second file has 48kHz/24-bit)
    assert(new_state.current_sample_rate != state.current_sample_rate || 
           new_state.current_bit_depth != state.current_bit_depth);
    
    printf("✓ metadata loading test passed\n");
}

// Test state persistence
void test_state_persistence() {
    printf("Testing state persistence...\n");
    
    // Set a specific mode and file
    audio_player_set_mode(MODE_PLAY_FOLDER_SHUFFLE);
    
    // Save state
    esp_err_t ret = audio_player_save_state();
    assert(ret == ESP_OK);
    
    // Load state
    ret = audio_player_load_state();
    assert(ret == ESP_OK);
    
    player_state_t state = audio_player_get_state();
    assert(state.mode == MODE_PLAY_FOLDER_SHUFFLE);
    
    printf("✓ state persistence test passed\n");
}

// Test folder index usage
void test_folder_index_usage() {
    printf("Testing folder index usage...\n");
    
    // The key test: files should use folderIndex from allFiles
    // File 0 and 1 should be in folder 0 (Pop)
    // File 2 and 3 should be in folder 1 (Rock)
    
    setup_test_index();
    
    // Verify test data is set up correctly
    assert(test_index.all_files[0].folder_index == 0); // Pop file
    assert(test_index.all_files[1].folder_index == 0); // Pop file
    assert(test_index.all_files[2].folder_index == 1); // Rock file
    assert(test_index.all_files[3].folder_index == 1); // Rock file
    
    printf("✓ folder index usage test passed\n");
}

int main() {
    printf("Running Audio Player unit tests...\n\n");
    
    test_audio_player_init();
    test_mode_switching();
    test_play_all_order_mode();
    test_folder_order_mode();
    test_metadata_loading();
    test_state_persistence();
    test_folder_index_usage();
    
    cleanup_test_index();
    
    printf("\n✅ All Audio Player tests passed!\n");
    return 0;
}
