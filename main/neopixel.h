#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <stdint.h>
#include "esp_err.h"
#include "audio_player.h"

// NeoPixel pin
#define NEOPIXEL_PIN    21

// Color definitions
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_color_t;

/**
 * @brief Initialize the NeoPixel
 * 
 * @return ESP_OK on success
 */
esp_err_t neopixel_init(void);

/**
 * @brief Set NeoPixel color
 * 
 * @param color RGB color values
 * @return ESP_OK on success
 */
esp_err_t neopixel_set_color(rgb_color_t color);

/**
 * @brief Turn off NeoPixel
 * 
 * @return ESP_OK on success
 */
esp_err_t neopixel_off(void);

/**
 * @brief Blink NeoPixel with a color
 * 
 * @param color RGB color values
 * @param duration_ms Blink duration in milliseconds
 * @return ESP_OK on success
 */
esp_err_t neopixel_blink(rgb_color_t color, uint32_t duration_ms);

/**
 * @brief Indicate current playback mode with NeoPixel
 * 
 * @param mode Playback mode to indicate
 * @return ESP_OK on success
 */
esp_err_t neopixel_indicate_mode(playback_mode_t mode);

#endif // NEOPIXEL_H
