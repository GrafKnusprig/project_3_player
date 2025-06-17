#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

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

#endif // AUDIO_PLAYER_H
