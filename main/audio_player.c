#include "audio_player.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#ifndef TEST_MODE
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#else
#define ESP_LOGI(tag, format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[WARN] " format "\n", ##__VA_ARGS__)

// Mock FreeRTOS types and definitions for test mode
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef int BaseType_t;
typedef void* i2s_chan_handle_t;
typedef struct { int dummy; } i2s_chan_config_t;
typedef struct { 
    struct {
        int sample_rate_hz;
        int clk_src;
        int mclk_multiple;
    } clk_cfg;
    struct {
        int data_bit_width;
        int slot_bit_width; 
        int slot_mode;
        int slot_mask;
        int ws_width;
        bool ws_pol;
        bool bit_shift;
    } slot_cfg;
    struct { 
        int bclk; 
        int ws; 
        int dout; 
        int din; 
        int mclk; 
    } gpio_cfg;
} i2s_std_config_t;
typedef int i2s_data_bit_width_t;
typedef int i2s_slot_mode_t;

#define pdPASS 1
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#define portMAX_DELAY 0xFFFFFFFF
#define tskIDLE_PRIORITY 0
#define I2S_CHANNEL_DEFAULT_CONFIG(port, role) {0}
#define I2S_PORT 0
#define I2S_ROLE_MASTER 0
#define I2S_NUM_0 0
#define I2S_CLK_SRC_DEFAULT 0
#define I2S_MCLK_MULTIPLE_256 0
#define I2S_DATA_BIT_WIDTH_8BIT 0
#define I2S_DATA_BIT_WIDTH_16BIT 1
#define I2S_DATA_BIT_WIDTH_24BIT 2
#define I2S_DATA_BIT_WIDTH_32BIT 3
#define I2S_SLOT_BIT_WIDTH_16BIT 1
#define I2S_SLOT_BIT_WIDTH_32BIT 3
#define I2S_SLOT_MODE_MONO 1
#define I2S_SLOT_MODE_STEREO 0
#define I2S_STD_SLOT_BOTH 0
#define I2S_GPIO_UNUSED -1

// Mock FreeRTOS functions
QueueHandle_t xQueueCreate(int items, int size) { return (void*)1; }
void vQueueDelete(QueueHandle_t queue) {}
int xQueueSend(QueueHandle_t queue, const void* item, int timeout) { return pdPASS; }
int xQueueReceive(QueueHandle_t queue, void* item, int timeout) { return pdTRUE; }
BaseType_t xTaskCreate(void* func, const char* name, int stack, void* param, int priority, TaskHandle_t* handle) { 
    *handle = (void*)1; 
    return pdPASS; 
}
void vTaskDelay(int ticks) {}
void vTaskDelete(TaskHandle_t task) {}

// Mock I2S functions
esp_err_t i2s_new_channel(i2s_chan_config_t* config, i2s_chan_handle_t* tx, i2s_chan_handle_t* rx) { return ESP_OK; }
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t handle, i2s_std_config_t* config) { return ESP_OK; }
esp_err_t i2s_channel_enable(i2s_chan_handle_t handle) { return ESP_OK; }
esp_err_t i2s_channel_disable(i2s_chan_handle_t handle) { return ESP_OK; }
esp_err_t i2s_del_channel(i2s_chan_handle_t handle) { return ESP_OK; }
esp_err_t i2s_channel_write(i2s_chan_handle_t handle, const void* src, size_t size, size_t* bytes_written, int timeout) {
    *bytes_written = size;
    return ESP_OK;
}

// Mock neopixel function
esp_err_t neopixel_indicate_mode(int mode) { return ESP_OK; }
#endif

#include "sd_card.h"
#include "pcm_file.h"
#include "json_parser.h"
#ifndef TEST_MODE
#include "neopixel.h"
#endif

static const char *TAG = "audio_player";

// I2S pins for PCM5102 DAC
#define I2S_DATA_PIN  22  // DIN
#define I2S_BCK_PIN   26  // BCK
#define I2S_LRCK_PIN  25  // LRC

// I2S configuration
#define I2S_PORT              I2S_NUM_0
#define I2S_SAMPLE_RATE       44100
#define I2S_BITS_PER_SAMPLE   16
#define I2S_CHANNELS          2
#define I2S_DMA_BUFFER_COUNT  8
#define I2S_DMA_BUFFER_LEN    1024

// Audio buffer size
#define AUDIO_BUFFER_SIZE     4096

// State file path
#define STATE_FILE_PATH       "/ESP32_MUSIC/player_state.bin"

// Player state and buffers
static player_state_t player_state;
static uint8_t audio_buffer[AUDIO_BUFFER_SIZE];
static pcm_file_t current_pcm_file;
static index_file_t music_index;

// Task handle for audio player
static TaskHandle_t player_task_handle = NULL;
static QueueHandle_t player_cmd_queue = NULL;

// Random seed initialized
static bool random_seed_initialized = false;

// Shuffle state
static int *shuffle_indices = NULL;
static int shuffle_count = 0;
static int shuffle_pos = 0;
// Forward declaration for shuffle list update
static void update_shuffle_list(void);

// Player commands
typedef enum {
    CMD_NONE = 0,
    CMD_PLAY,
    CMD_STOP,
    CMD_NEXT,
    CMD_PREV,
    CMD_NEXT_FOLDER,
    CMD_PREV_FOLDER,
    CMD_CHANGE_MODE,
    CMD_QUIT
} player_cmd_t;

// Forward declarations
static void player_task(void *arg);
static esp_err_t play_file(const char *filepath);
static esp_err_t select_next_file(void);
static esp_err_t select_prev_file(void);
static esp_err_t select_next_folder(void);
static esp_err_t select_prev_folder(void);
static void update_current_folder_index_for_file(const char *filepath);
static esp_err_t configure_i2s(uint32_t sample_rate, uint16_t bit_depth, uint16_t channels);

// Add static handle for I2S TX channel
static i2s_chan_handle_t i2s_tx_chan = NULL;

// Current I2S configuration
static uint32_t current_i2s_sample_rate = 0;
static uint16_t current_i2s_bit_depth = 0;
static uint16_t current_i2s_channels = 0;

esp_err_t audio_player_init(void) {
    ESP_LOGI(TAG, "Initializing audio player");
    
    // Check if SD card is mounted
    if (!sd_card_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_FAIL;
    }

    // Try to load state first
    esp_err_t state_ret = audio_player_load_state();
    if (state_ret != ESP_OK) {
        // If load failed, set defaults
        player_state.mode = MODE_PLAY_ALL_ORDER;
        player_state.current_file_index = 0;
        player_state.current_folder_index = 0;
        player_state.is_playing = false;
        memset(player_state.current_file_path, 0, sizeof(player_state.current_file_path));
    }

    // Initialize I2S for audio output (fixed for ESP-IDF v5+)
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = I2S_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_SLOT_BIT_WIDTH_32BIT, // Use 32-bit WS width for standard I2S
            .ws_pol = false,
            .bit_shift = true // Enable bit shift for standard I2S
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_PIN,
            .ws = I2S_LRCK_PIN,
            .dout = I2S_DATA_PIN,
            .din = I2S_GPIO_UNUSED
        }
    };
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, &i2s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S TX channel");
        return ret;
    }
    ret = i2s_channel_init_std_mode(i2s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard channel");
        return ret;
    }
    // Enable the I2S TX channel
    ret = i2s_channel_enable(i2s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX channel");
        return ret;
    }
    
    // Parse index.json file
    char index_path[256];
    const char* mount_point = sd_card_get_mount_point();
    
    // Debug print the mount point
    ESP_LOGI(TAG, "SD card mount point: %s", mount_point);
    
    // Use the proper long filename with the standard mount point
    snprintf(index_path, sizeof(index_path), "%s/ESP32_MUSIC/index.json", mount_point);
    
    ESP_LOGI(TAG, "Looking for index file at: %s", index_path);
    
    // Parse the index file
    ret = json_parse_index(index_path, &music_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse index.json file - continuing without index");
        // Initialize an empty index to avoid null pointers
        memset(&music_index, 0, sizeof(index_file_t));
        // Don't return error - we'll continue without index
    } else {
        ESP_LOGI(TAG, "Successfully loaded index with %d files", music_index.total_files);
    }
    
    // Create command queue
    player_cmd_queue = xQueueCreate(10, sizeof(player_cmd_t));
    if (player_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create player command queue");
        json_free_index(&music_index);
        if (i2s_tx_chan) {
            i2s_del_channel(i2s_tx_chan);
            i2s_tx_chan = NULL;
        }
        return ESP_ERR_NO_MEM;
    }
    
    // Create player task
    BaseType_t task_created = xTaskCreate(
        player_task,
        "player_task",
        4096,
        NULL,
        tskIDLE_PRIORITY + 2,
        &player_task_handle
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create player task");
        vQueueDelete(player_cmd_queue);
        json_free_index(&music_index);
        if (i2s_tx_chan) {
            i2s_del_channel(i2s_tx_chan);
            i2s_tx_chan = NULL;
        }
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Audio player initialized successfully");
    return ESP_OK;
}

esp_err_t audio_player_start(void) {
    if (player_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    player_cmd_t cmd = CMD_PLAY;
    if (xQueueSend(player_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send play command to queue");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t audio_player_stop(void) {
    if (player_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    player_cmd_t cmd = CMD_STOP;
    if (xQueueSend(player_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send stop command to queue");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t audio_player_seek(size_t byte_pos) {
    if (current_pcm_file.file == NULL) {
        ESP_LOGW(TAG, "No file is currently open for seeking");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = pcm_file_seek(&current_pcm_file, byte_pos);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to seek to position %zu", byte_pos);
        return ret;
    }

    ESP_LOGI(TAG, "Seeked to byte position %zu", byte_pos);
    return ESP_OK;
}

esp_err_t audio_player_next(void) {
    if (player_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    player_cmd_t cmd = CMD_NEXT;
    if (xQueueSend(player_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send next command to queue");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t audio_player_prev(void) {
    if (player_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    player_cmd_t cmd = CMD_PREV;
    if (xQueueSend(player_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send prev command to queue");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t audio_player_next_folder(void) {
    if (player_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    player_cmd_t cmd = CMD_NEXT_FOLDER;
    if (xQueueSend(player_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send next folder command to queue");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t audio_player_prev_folder(void) {
    if (player_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    player_cmd_t cmd = CMD_PREV_FOLDER;
    if (xQueueSend(player_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send prev folder command to queue");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t audio_player_set_mode(playback_mode_t mode) {
    if (mode >= MODE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    // If switching to a folder mode, update current_folder_index to match the current file
    if ((mode == MODE_PLAY_FOLDER_ORDER || mode == MODE_PLAY_FOLDER_SHUFFLE) && strlen(player_state.current_file_path) > 0) {
        update_current_folder_index_for_file(player_state.current_file_path);
    }
    player_state.mode = mode;
    // Update shuffle list if entering a shuffle mode
    update_shuffle_list();
    // Indicate mode with NeoPixel
    neopixel_indicate_mode(mode);
    // Save state
    audio_player_save_state();
    return ESP_OK;
}

player_state_t audio_player_get_state(void) {
    return player_state;
}

esp_err_t audio_player_save_state(void) {
    if (!sd_card_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    return sd_card_write_file(STATE_FILE_PATH, &player_state, sizeof(player_state));
}

esp_err_t audio_player_load_state(void) {
    if (!sd_card_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    size_t bytes_read = 0;
    esp_err_t ret = sd_card_read_file(STATE_FILE_PATH, &player_state, sizeof(player_state), &bytes_read);
    
    if (ret == ESP_OK && bytes_read == sizeof(player_state)) {
        // Validate values
        if (player_state.mode >= MODE_MAX) {
            player_state.mode = MODE_PLAY_ALL_ORDER;
        }
        
        if (player_state.current_folder_index >= music_index.folder_count) {
            player_state.current_folder_index = 0;
        }
        
        ESP_LOGI(TAG, "Loaded saved state: mode=%d, file=%s", 
            player_state.mode, player_state.current_file_path);
        
        return ESP_OK;
    }
    
    // If load failed, use default values
    player_state.mode = MODE_PLAY_ALL_ORDER;
    player_state.current_file_index = 0;
    player_state.current_folder_index = 0;
    player_state.is_playing = false;
    memset(player_state.current_file_path, 0, sizeof(player_state.current_file_path));
    
    ESP_LOGI(TAG, "Using default player state");
    return ESP_ERR_NOT_FOUND;
}

// Helper: Update current_folder_index to match the folder containing the given file path
static void update_current_folder_index_for_file(const char *filepath);

// Player task function
static void player_task(void *arg) {
    ESP_LOGI(TAG, "Player task started");
    
    player_cmd_t cmd;
    bool running = true;
    
    // Initialize random number generator if not already done
    if (!random_seed_initialized) {
        srand(time(NULL));
        random_seed_initialized = true;
    }
    
    // If we have a saved file path, try to play it
    if (strlen(player_state.current_file_path) > 0) {
        // If shuffle mode, regenerate shuffle list
        update_shuffle_list();
        play_file(player_state.current_file_path);
    } else if (music_index.total_files > 0 && music_index.all_files != NULL) {
        // Otherwise, start with first file
        update_shuffle_list();
        char full_path[256];
        json_get_full_path(music_index.all_files[0].path, full_path, sizeof(full_path));
        play_file(full_path);
    } else {
        ESP_LOGW(TAG, "No music files in index - waiting for user action");
        // Set state to show we're not currently playing anything
        player_state.is_playing = false;
        memset(player_state.current_file_path, 0, sizeof(player_state.current_file_path));
    }
    
    // Task loop
    while (running) {
        if (xQueueReceive(player_cmd_queue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            switch (cmd) {
                case CMD_PLAY:
                    player_state.is_playing = true;
                    ESP_LOGI(TAG, "Play command received");
                    break;
                    
                case CMD_STOP:
                    player_state.is_playing = false;
                    ESP_LOGI(TAG, "Stop command received");
                    break;
                    
                case CMD_NEXT:
                    ESP_LOGI(TAG, "Next command received");
                    select_next_file();
                    break;
                    
                case CMD_PREV:
                    ESP_LOGI(TAG, "Previous command received");
                    select_prev_file();
                    break;
                    
                case CMD_NEXT_FOLDER:
                    ESP_LOGI(TAG, "Next folder command received");
                    select_next_folder();
                    break;
                    
                case CMD_PREV_FOLDER:
                    ESP_LOGI(TAG, "Previous folder command received");
                    select_prev_folder();
                    break;
                    
                case CMD_CHANGE_MODE:
                    ESP_LOGI(TAG, "Change mode command received");
                    // Cycle through modes
                    player_state.mode = (player_state.mode + 1) % MODE_MAX;
                    audio_player_set_mode(player_state.mode);
                    break;
                    
                case CMD_QUIT:
                    ESP_LOGI(TAG, "Quit command received");
                    running = false;
                    break;
                    
                default:
                    break;
            }
        }
        
        // Handle playback
        if (player_state.is_playing) {
            // Only read audio data if file is open
            if (current_pcm_file.file != NULL) {
                size_t bytes_read = 0;
                esp_err_t ret = pcm_file_read(&current_pcm_file, audio_buffer, AUDIO_BUFFER_SIZE, &bytes_read);
                
                if (ret == ESP_OK && bytes_read > 0) {
                    // Write data to I2S
                    size_t bytes_written = 0;
                    esp_err_t i2s_ret = i2s_channel_write(i2s_tx_chan, audio_buffer, bytes_read, &bytes_written, portMAX_DELAY);
                    if (i2s_ret != ESP_OK) {
                        ESP_LOGE(TAG, "i2s_channel_write failed: %d", i2s_ret);
                    }
                    
                    // Check if all bytes were written
                    if (bytes_written != bytes_read) {
                        ESP_LOGW(TAG, "Not all bytes written to I2S: %zu of %zu", 
                                  bytes_written, bytes_read);
                    }
                } else {
                    // End of file or error
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Error reading PCM file");
                    }
                    
                    // Close current file
                    pcm_file_close(&current_pcm_file);
                    
                    // Play next file
                    select_next_file();
                }
            } else {
                // No file is open, try to find one to play
                select_next_file();
                
                // Small delay to prevent CPU hogging if no file is found
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        } else {
            // Not playing, just delay
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    // Clean up
    if (current_pcm_file.file != NULL) {
        pcm_file_close(&current_pcm_file);
    }
    
    ESP_LOGI(TAG, "Player task ended");
    vTaskDelete(NULL);
}

// Play a specific file
static esp_err_t play_file(const char *filepath) {
    ESP_LOGI(TAG, "Playing file: %s", filepath);
    
    // Find the file entry in the index to get metadata
    file_entry_t *file_entry = NULL;
    const char *mount_point = sd_card_get_mount_point();
    const char *rel_path = filepath;
    
    // Strip mount point and /ESP32_MUSIC/ prefix to get relative path
    if (strncmp(filepath, mount_point, strlen(mount_point)) == 0) {
        rel_path = filepath + strlen(mount_point);
        if (*rel_path == '/') rel_path++;
        if (strncmp(rel_path, "ESP32_MUSIC/", 12) == 0) {
            rel_path += 12;
        }
    }
    
    // Find the file in the index
    for (int i = 0; i < music_index.total_files; i++) {
        if (strcmp(music_index.all_files[i].path, rel_path) == 0) {
            file_entry = &music_index.all_files[i];
            break;
        }
    }
    
    if (file_entry == NULL) {
        ESP_LOGE(TAG, "File not found in index: %s", rel_path);
        return ESP_FAIL;
    }
    
    // Configure I2S for this file's audio parameters
    esp_err_t ret = configure_i2s(file_entry->sample_rate, file_entry->bit_depth, file_entry->channels);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2S for file");
        return ret;
    }
    
    // Close any open file
    if (current_pcm_file.file != NULL) {
        pcm_file_close(&current_pcm_file);
    }
    
    // Open the new file with metadata
    ret = pcm_file_open(filepath, &current_pcm_file, file_entry->sample_rate, file_entry->bit_depth, file_entry->channels);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open PCM file");
        return ret;
    }
    
    // Update the player state with current song metadata
    strncpy(player_state.current_file_path, filepath, sizeof(player_state.current_file_path) - 1);
    player_state.current_file_path[sizeof(player_state.current_file_path) - 1] = '\0';
    
    strncpy(player_state.current_song, file_entry->song, sizeof(player_state.current_song) - 1);
    player_state.current_song[sizeof(player_state.current_song) - 1] = '\0';
    
    strncpy(player_state.current_album, file_entry->album, sizeof(player_state.current_album) - 1);
    player_state.current_album[sizeof(player_state.current_album) - 1] = '\0';
    
    strncpy(player_state.current_artist, file_entry->artist, sizeof(player_state.current_artist) - 1);
    player_state.current_artist[sizeof(player_state.current_artist) - 1] = '\0';
    
    player_state.current_sample_rate = file_entry->sample_rate;
    player_state.current_bit_depth = file_entry->bit_depth;
    player_state.current_channels = file_entry->channels;
    
    ESP_LOGI(TAG, "Now playing: %s by %s from %s", 
             player_state.current_song, player_state.current_artist, player_state.current_album);
    ESP_LOGI(TAG, "Audio format: %u Hz, %u-bit, %u channels", 
             player_state.current_sample_rate, player_state.current_bit_depth, player_state.current_channels);
    
    // Save state
    audio_player_save_state();
    
    return ESP_OK;
}

// Helper: Update current_folder_index to match the folder containing the given file path
static void update_current_folder_index_for_file(const char *filepath) {
    ESP_LOGI(TAG, "update_current_folder_index_for_file: looking for %s", filepath);
    
    // Strip mount point and /ESP32_MUSIC/ prefix to get relative path
    const char *mount_point = sd_card_get_mount_point();
    const char *rel_path = filepath;
    if (strncmp(filepath, mount_point, strlen(mount_point)) == 0) {
        rel_path = filepath + strlen(mount_point);
        if (*rel_path == '/') rel_path++;
        if (strncmp(rel_path, "ESP32_MUSIC/", 12) == 0) {
            rel_path += 12;
        }
    }
    
    ESP_LOGI(TAG, "Relative path for index lookup: %s", rel_path);
    
    // Find the file in allFiles and use its folderIndex
    for (int i = 0; i < music_index.total_files; i++) {
        if (strcmp(music_index.all_files[i].path, rel_path) == 0) {
            player_state.current_folder_index = music_index.all_files[i].folder_index;
            
            // Now find the file index within that folder
            if (player_state.current_folder_index < music_index.folder_count) {
                folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
                for (int j = 0; j < folder->file_count; j++) {
                    if (strcmp(folder->files[j].path, rel_path) == 0) {
                        player_state.current_file_index = j;
                        ESP_LOGI(TAG, "Found file in folder %d, file %d: %s", 
                                 player_state.current_folder_index, j, folder->files[j].path);
                        return;
                    }
                }
            }
            
            ESP_LOGI(TAG, "Found file in allFiles with folder index %d: %s", 
                     player_state.current_folder_index, rel_path);
            return;
        }
    }
    
    ESP_LOGW(TAG, "File not found in index: %s", rel_path);
}

// Helper to free shuffle indices
static void free_shuffle_indices(void) {
    if (shuffle_indices) {
        free(shuffle_indices);
        shuffle_indices = NULL;
        shuffle_count = 0;
        shuffle_pos = 0;
    }
}

// Fisher-Yates shuffle
static void shuffle_array(int *array, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
}

// Generate shuffle list for all files
static void generate_shuffle_all(void) {
    free_shuffle_indices();
    if (music_index.total_files <= 0) return;
    shuffle_count = music_index.total_files;
    shuffle_indices = malloc(sizeof(int) * shuffle_count);
    for (int i = 0; i < shuffle_count; ++i) shuffle_indices[i] = i;
    shuffle_array(shuffle_indices, shuffle_count);
    shuffle_pos = 0;
}

// Generate shuffle list for current folder
static void generate_shuffle_folder(void) {
    free_shuffle_indices();
    if (music_index.folder_count == 0 || player_state.current_folder_index >= music_index.folder_count) return;
    folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
    if (folder->file_count <= 0) return;
    shuffle_count = folder->file_count;
    shuffle_indices = malloc(sizeof(int) * shuffle_count);
    for (int i = 0; i < shuffle_count; ++i) shuffle_indices[i] = i;
    shuffle_array(shuffle_indices, shuffle_count);
    shuffle_pos = 0;
}

// Call this whenever mode is set or folder changes
static void update_shuffle_list(void) {
    if (player_state.mode == MODE_PLAY_ALL_SHUFFLE) {
        generate_shuffle_all();
    } else if (player_state.mode == MODE_PLAY_FOLDER_SHUFFLE) {
        generate_shuffle_folder();
    } else {
        free_shuffle_indices();
    }
}

// Select and play next file based on current mode
static esp_err_t select_next_file(void) {
    if (music_index.total_files == 0 || music_index.all_files == NULL) {
        ESP_LOGW(TAG, "No files in index or all_files is NULL");
        return ESP_FAIL;
    }
    char full_path[256];
    if (player_state.mode == MODE_PLAY_ALL_ORDER) {
        // All files in order
        player_state.current_file_index = (player_state.current_file_index + 1) % music_index.total_files;
        json_get_full_path(music_index.all_files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else if (player_state.mode == MODE_PLAY_ALL_SHUFFLE) {
        if (!shuffle_indices || shuffle_count != music_index.total_files) {
            generate_shuffle_all();
        }
        shuffle_pos = (shuffle_pos + 1) % shuffle_count;
        player_state.current_file_index = shuffle_indices[shuffle_pos];
        json_get_full_path(music_index.all_files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else if (player_state.mode == MODE_PLAY_FOLDER_ORDER) {
        if (music_index.folder_count == 0 || player_state.current_folder_index >= music_index.folder_count) {
            ESP_LOGW(TAG, "No folders or invalid folder index");
            return ESP_FAIL;
        }
        folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
        if (folder->file_count == 0) {
            ESP_LOGW(TAG, "No files in folder");
            return ESP_FAIL;
        }
        player_state.current_file_index = (player_state.current_file_index + 1) % folder->file_count;
        json_get_full_path(folder->files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else if (player_state.mode == MODE_PLAY_FOLDER_SHUFFLE) {
        if (music_index.folder_count == 0 || player_state.current_folder_index >= music_index.folder_count) {
            ESP_LOGW(TAG, "No folders or invalid folder index");
            return ESP_FAIL;
        }
        folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
        if (!shuffle_indices || shuffle_count != folder->file_count) {
            generate_shuffle_folder();
        }
        shuffle_pos = (shuffle_pos + 1) % shuffle_count;
        player_state.current_file_index = shuffle_indices[shuffle_pos];
        json_get_full_path(folder->files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else {
        ESP_LOGW(TAG, "Unknown mode");
        return ESP_FAIL;
    }
    return play_file(full_path);
}

// Select and play previous file
static esp_err_t select_prev_file(void) {
    if (music_index.total_files == 0) {
        ESP_LOGW(TAG, "No files in index");
        return ESP_FAIL;
    }
    char full_path[256];
    if (player_state.mode == MODE_PLAY_ALL_ORDER) {
        player_state.current_file_index = (player_state.current_file_index == 0) ? (music_index.total_files - 1) : (player_state.current_file_index - 1);
        json_get_full_path(music_index.all_files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else if (player_state.mode == MODE_PLAY_ALL_SHUFFLE) {
        if (!shuffle_indices || shuffle_count != music_index.total_files) {
            generate_shuffle_all();
        }
        shuffle_pos = (shuffle_pos == 0) ? (shuffle_count - 1) : (shuffle_pos - 1);
        player_state.current_file_index = shuffle_indices[shuffle_pos];
        json_get_full_path(music_index.all_files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else if (player_state.mode == MODE_PLAY_FOLDER_ORDER) {
        if (music_index.folder_count == 0 || player_state.current_folder_index >= music_index.folder_count) {
            ESP_LOGW(TAG, "No folders or invalid folder index");
            return ESP_FAIL;
        }
        folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
        if (folder->file_count == 0) {
            ESP_LOGW(TAG, "No files in folder");
            return ESP_FAIL;
        }
        player_state.current_file_index = (player_state.current_file_index == 0) ? (folder->file_count - 1) : (player_state.current_file_index - 1);
        json_get_full_path(folder->files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else if (player_state.mode == MODE_PLAY_FOLDER_SHUFFLE) {
        if (music_index.folder_count == 0 || player_state.current_folder_index >= music_index.folder_count) {
            ESP_LOGW(TAG, "No folders or invalid folder index");
            return ESP_FAIL;
        }
        folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
        if (!shuffle_indices || shuffle_count != folder->file_count) {
            generate_shuffle_folder();
        }
        shuffle_pos = (shuffle_pos == 0) ? (shuffle_count - 1) : (shuffle_pos - 1);
        player_state.current_file_index = shuffle_indices[shuffle_pos];
        json_get_full_path(folder->files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else {
        ESP_LOGW(TAG, "Unknown mode");
        return ESP_FAIL;
    }
    return play_file(full_path);
}

// Select and play next folder
static esp_err_t select_next_folder(void) {
    if (music_index.folder_count == 0) {
        ESP_LOGW(TAG, "No folders in index");
        return ESP_FAIL;
    }
    // Move to next folder
    player_state.current_folder_index = (player_state.current_folder_index + 1) % music_index.folder_count;
    // Reset file index
    player_state.current_file_index = 0;
    // Regenerate shuffle list if in folder shuffle mode
    if (player_state.mode == MODE_PLAY_FOLDER_SHUFFLE) {
        generate_shuffle_folder();
    }
    // Play first file in folder (or first in shuffle)
    folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
    if (folder->file_count == 0) {
        ESP_LOGW(TAG, "No files in folder");
        return ESP_FAIL;
    }
    char full_path[256];
    if (player_state.mode == MODE_PLAY_FOLDER_SHUFFLE && shuffle_indices && shuffle_count > 0) {
        player_state.current_file_index = shuffle_indices[0];
        json_get_full_path(folder->files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else {
        json_get_full_path(folder->files[0].path, full_path, sizeof(full_path));
    }
    return play_file(full_path);
}

// Select and play previous folder
static esp_err_t select_prev_folder(void) {
    if (music_index.folder_count == 0) {
        ESP_LOGW(TAG, "No folders in index");
        return ESP_FAIL;
    }
    // Move to previous folder
    player_state.current_folder_index = (player_state.current_folder_index == 0) ? (music_index.folder_count - 1) : (player_state.current_folder_index - 1);
    // Reset file index
    player_state.current_file_index = 0;
    // Regenerate shuffle list if in folder shuffle mode
    if (player_state.mode == MODE_PLAY_FOLDER_SHUFFLE) {
        generate_shuffle_folder();
    }
    // Play first file in folder (or first in shuffle)
    folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
    if (folder->file_count == 0) {
        ESP_LOGW(TAG, "No files in folder");
        return ESP_FAIL;
    }
    char full_path[256];
    if (player_state.mode == MODE_PLAY_FOLDER_SHUFFLE && shuffle_indices && shuffle_count > 0) {
        player_state.current_file_index = shuffle_indices[0];
        json_get_full_path(folder->files[player_state.current_file_index].path, full_path, sizeof(full_path));
    } else {
        json_get_full_path(folder->files[0].path, full_path, sizeof(full_path));
    }
    return play_file(full_path);
}

// Function to configure I2S for specific audio parameters
static esp_err_t configure_i2s(uint32_t sample_rate, uint16_t bit_depth, uint16_t channels) {
    // Check if we need to reconfigure
    if (current_i2s_sample_rate == sample_rate && 
        current_i2s_bit_depth == bit_depth && 
        current_i2s_channels == channels) {
        return ESP_OK; // No change needed
    }
    
    ESP_LOGI(TAG, "Configuring I2S: %u Hz, %u bits, %u channels", sample_rate, bit_depth, channels);
    
    // Disable current channel if exists
    if (i2s_tx_chan) {
        i2s_channel_disable(i2s_tx_chan);
        i2s_del_channel(i2s_tx_chan);
        i2s_tx_chan = NULL;
    }
    
    // Create new I2S configuration
    i2s_data_bit_width_t bit_width;
    switch (bit_depth) {
        case 8:  bit_width = I2S_DATA_BIT_WIDTH_8BIT; break;
        case 16: bit_width = I2S_DATA_BIT_WIDTH_16BIT; break;
        case 24: bit_width = I2S_DATA_BIT_WIDTH_24BIT; break;
        case 32: bit_width = I2S_DATA_BIT_WIDTH_32BIT; break;
        default: bit_width = I2S_DATA_BIT_WIDTH_16BIT; break;
    }
    
    i2s_slot_mode_t slot_mode = (channels == 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = bit_width,
            .slot_bit_width = bit_width,
            .slot_mode = slot_mode,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .ws_pol = false,
            .bit_shift = true
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_PIN,
            .ws = I2S_LRCK_PIN,
            .dout = I2S_DATA_PIN,
            .din = I2S_GPIO_UNUSED
        }
    };
    
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, &i2s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S TX channel");
        return ret;
    }
    
    ret = i2s_channel_init_std_mode(i2s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard channel");
        i2s_del_channel(i2s_tx_chan);
        i2s_tx_chan = NULL;
        return ret;
    }
    
    ret = i2s_channel_enable(i2s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX channel");
        i2s_del_channel(i2s_tx_chan);
        i2s_tx_chan = NULL;
        return ret;
    }
    
    // Update current configuration
    current_i2s_sample_rate = sample_rate;
    current_i2s_bit_depth = bit_depth;
    current_i2s_channels = channels;
    
    ESP_LOGI(TAG, "I2S configured successfully");
    return ESP_OK;
}

#ifdef TEST_MODE
// Test helper function to manually trigger file selection
esp_err_t test_select_next_file(void) {
    return select_next_file();
}

esp_err_t test_select_prev_file(void) {
    return select_prev_file();
}

esp_err_t test_play_current_file(void) {
    // Get current file path
    char filepath[256];
    esp_err_t ret = json_get_full_path(music_index.all_files[player_state.current_file_index].path, 
                                       filepath, sizeof(filepath));
    if (ret != ESP_OK) {
        return ret;
    }
    
    return play_file(filepath);
}
#endif
