#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef TEST_MODE
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_NO_MEM -3
#define ESP_ERR_INVALID_STATE -4
#define ESP_ERR_NOT_FOUND -5
#else
#include "esp_err.h"
#endif

// Playback mode definitions
typedef enum {
    MODE_PLAY_ALL_ORDER = 0,      // Play all files in order
    MODE_PLAY_ALL_SHUFFLE,        // Play all files in random order
    MODE_PLAY_FOLDER_ORDER,       // Play current folder in order
    MODE_PLAY_FOLDER_SHUFFLE,     // Play current folder in random order
    MODE_MAX
} playback_mode_t;

// Player state
typedef struct {
    playback_mode_t mode;
    int current_file_index;
    int current_folder_index;
    bool is_playing;
    char current_file_path[256];
    // Current song metadata
    char current_song[256];
    char current_album[256];
    char current_artist[256];
    uint32_t current_sample_rate;
    uint16_t current_bit_depth;
    uint16_t current_channels;
} player_state_t;

/**
 * @brief Initialize the audio player
 * 
 * @return ESP_OK on success
 */
esp_err_t audio_player_init(void);

/**
 * @brief Start playback
 * 
 * @return ESP_OK on success
 */
esp_err_t audio_player_start(void);

/**
 * @brief Stop playback
 * 
 * @return ESP_OK on success
 */
esp_err_t audio_player_stop(void);

/**
 * @brief Play next track
 * 
 * @return ESP_OK on success
 */
esp_err_t audio_player_next(void);

/**
 * @brief Play previous track
 * 
 * @return ESP_OK on success
 */
esp_err_t audio_player_prev(void);

/**
 * @brief Play next folder (for folder modes)
 * 
 * @return ESP_OK on success
 */
esp_err_t audio_player_next_folder(void);

/**
 * @brief Play previous folder (for folder modes)
 * 
 * @return ESP_OK on success
 */
esp_err_t audio_player_prev_folder(void);

/**
 * @brief Change playback mode
 * 
 * @param mode The new playback mode
 * @return ESP_OK on success
 */
esp_err_t audio_player_set_mode(playback_mode_t mode);

/**
 * @brief Get current player state
 * 
 * @return Current player state
 */
player_state_t audio_player_get_state(void);

/**
 * @brief Save player state to SD card
 * 
 * @return ESP_OK on success
 */
esp_err_t audio_player_save_state(void);

/**
 * @brief Load player state from SD card
 * 
 * @return ESP_OK on success
 */
esp_err_t audio_player_load_state(void);

/**
 * @brief Seek to a specific position in the current track
 * 
 * @param byte_position Position to seek to, in bytes
 * @return ESP_OK on success
 */
esp_err_t audio_player_seek(size_t byte_position);

#endif // AUDIO_PLAYER_H
