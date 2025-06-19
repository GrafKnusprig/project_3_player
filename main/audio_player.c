#include "audio_player.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "sd_card.h"
#include "pcm_file.h"
#include "json_parser.h"
#include "neopixel.h"

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

// Add static handle for I2S TX channel
static i2s_chan_handle_t i2s_tx_chan = NULL;

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
    
    player_state.mode = mode;
    
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
        play_file(player_state.current_file_path);
    } else if (music_index.total_files > 0 && music_index.all_files != NULL) {
        // Otherwise, start with first file
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
                        ESP_LOGW(TAG, "Not all bytes written to I2S: %d of %d", 
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
    
    // Close any open file
    if (current_pcm_file.file != NULL) {
        pcm_file_close(&current_pcm_file);
    }
    
    // Open the new file
    esp_err_t ret = pcm_file_open(filepath, &current_pcm_file);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open PCM file");
        return ret;
    }
    
    // Update the current file path
    strncpy(player_state.current_file_path, filepath, sizeof(player_state.current_file_path) - 1);
    player_state.current_file_path[sizeof(player_state.current_file_path) - 1] = '\0';
    // Update folder index to match the file
    update_current_folder_index_for_file(filepath);
    // Save state
    audio_player_save_state();
    
    return ESP_OK;
}

// Helper: Update current_folder_index to match the folder containing the given file path
static void update_current_folder_index_for_file(const char *filepath) {
    for (int i = 0; i < music_index.folder_count; ++i) {
        folder_t *folder = &music_index.music_folders[i];
        for (int j = 0; j < folder->file_count; ++j) {
            char abs_path[256];
            json_get_full_path(folder->files[j].path, abs_path, sizeof(abs_path));
            if (strcmp(abs_path, filepath) == 0) {
                player_state.current_folder_index = i;
                player_state.current_file_index = j;
                return;
            }
        }
    }
}

// Select and play next file based on current mode
static esp_err_t select_next_file(void) {
    // No files in index
    if (music_index.total_files == 0 || music_index.all_files == NULL) {
        ESP_LOGW(TAG, "No files in index or all_files is NULL");
        return ESP_FAIL;
    }
    char full_path[256];
    bool random_mode = (player_state.mode == MODE_PLAY_ALL_SHUFFLE) || (player_state.mode == MODE_PLAY_FOLDER_SHUFFLE);
    // Handle different modes
    if (player_state.mode == MODE_PLAY_ALL_ORDER || player_state.mode == MODE_PLAY_ALL_SHUFFLE) {
        // All files mode
        if (random_mode) {
            // Play random file
            player_state.current_file_index = rand() % music_index.total_files;
        } else {
            // Play next file in sequence
            player_state.current_file_index = (player_state.current_file_index + 1) % music_index.total_files;
        }
        // Check bounds to prevent crashes
        if (player_state.current_file_index < 0 || player_state.current_file_index >= music_index.total_files) {
            ESP_LOGE(TAG, "Invalid file index: %d (max: %d)", 
                player_state.current_file_index, music_index.total_files - 1);
            player_state.current_file_index = 0;  // Reset to first file
        }
        // Get file path
        json_get_full_path(music_index.all_files[player_state.current_file_index].path, 
                          full_path, sizeof(full_path));
    } else {
        // Folder mode
        if (music_index.folder_count == 0 || 
            player_state.current_folder_index >= music_index.folder_count) {
            ESP_LOGW(TAG, "No folders or invalid folder index");
            return ESP_FAIL;
        }
        folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
        if (folder->file_count == 0) {
            ESP_LOGW(TAG, "No files in folder");
            return ESP_FAIL;
        }
        int file_idx;
        if (random_mode) {
            // Play random file in folder
            file_idx = rand() % folder->file_count;
        } else {
            // Play next file in folder
            file_idx = (player_state.current_file_index + 1) % folder->file_count;
        }
        player_state.current_file_index = file_idx;
        // Get file path
        json_get_full_path(folder->files[file_idx].path, full_path, sizeof(full_path));
    }
    // Play the file
    return play_file(full_path);
}

// Select and play previous file
static esp_err_t select_prev_file(void) {
    // No files in index
    if (music_index.total_files == 0) {
        ESP_LOGW(TAG, "No files in index");
        return ESP_FAIL;
    }
    
    char full_path[256];
    
    // Handle different modes
    if (player_state.mode == MODE_PLAY_ALL_ORDER || player_state.mode == MODE_PLAY_ALL_SHUFFLE) {
        // All files mode - always sequential for prev
        player_state.current_file_index = (player_state.current_file_index == 0) ? 
                                        (music_index.total_files - 1) : 
                                        (player_state.current_file_index - 1);
        
        // Get file path
        json_get_full_path(music_index.all_files[player_state.current_file_index].path, 
                          full_path, sizeof(full_path));
    } else {
        // Folder mode
        if (music_index.folder_count == 0 || 
            player_state.current_folder_index >= music_index.folder_count) {
            ESP_LOGW(TAG, "No folders or invalid folder index");
            return ESP_FAIL;
        }
        
        folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
        
        if (folder->file_count == 0) {
            ESP_LOGW(TAG, "No files in folder");
            return ESP_FAIL;
        }
        
        // Go to previous file in folder
        player_state.current_file_index = (player_state.current_file_index == 0) ?
                                        (folder->file_count - 1) :
                                        (player_state.current_file_index - 1);
        
        // Get file path
        json_get_full_path(folder->files[player_state.current_file_index].path, 
                          full_path, sizeof(full_path));
    }
    
    // Play the file
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
    
    // Play first file in folder
    folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
    if (folder->file_count == 0) {
        ESP_LOGW(TAG, "No files in folder");
        return ESP_FAIL;
    }
    
    char full_path[256];
    json_get_full_path(folder->files[0].path, full_path, sizeof(full_path));
    
    return play_file(full_path);
}

// Select and play previous folder
static esp_err_t select_prev_folder(void) {
    if (music_index.folder_count == 0) {
        ESP_LOGW(TAG, "No folders in index");
        return ESP_FAIL;
    }
    
    // Move to previous folder
    player_state.current_folder_index = (player_state.current_folder_index == 0) ?
                                      (music_index.folder_count - 1) :
                                      (player_state.current_folder_index - 1);
    
    // Reset file index
    player_state.current_file_index = 0;
    
    // Play first file in folder
    folder_t *folder = &music_index.music_folders[player_state.current_folder_index];
    if (folder->file_count == 0) {
        ESP_LOGW(TAG, "No files in folder");
        return ESP_FAIL;
    }
    
    char full_path[256];
    json_get_full_path(folder->files[0].path, full_path, sizeof(full_path));
    
    return play_file(full_path);
}
