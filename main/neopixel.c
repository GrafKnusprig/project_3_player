#include "neopixel.h"
#include <stdlib.h>
#include <string.h>
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "neopixel";

// NeoPixel WS2812 timing parameters
#define WS2812_T0H_NS 350    // 0 bit high time
#define WS2812_T0L_NS 900    // 0 bit low time
#define WS2812_T1H_NS 900    // 1 bit high time
#define WS2812_T1L_NS 350    // 1 bit low time
#define WS2812_RESET_US 80   // Reset time in microseconds

// RMT transmitter handle
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

// Mode colors with reduced brightness for comfort
static const rgb_color_t mode_colors[MODE_MAX] = {
    {50, 0, 0},     // Red - MODE_PLAY_ALL_ORDER
    {0, 50, 0},     // Green - MODE_PLAY_ALL_SHUFFLE
    {0, 0, 50},     // Blue - MODE_PLAY_FOLDER_ORDER
    {50, 50, 0}    // Yellow - MODE_PLAY_FOLDER_SHUFFLE
};

// Global brightness setting (0-100%)
static uint8_t neopixel_brightness = 20; // 20% brightness by default

// Function to scale RGB values by brightness
static rgb_color_t neopixel_scale_brightness(rgb_color_t color) {
    rgb_color_t scaled;
    scaled.red = (color.red * neopixel_brightness) / 100;
    scaled.green = (color.green * neopixel_brightness) / 100;
    scaled.blue = (color.blue * neopixel_brightness) / 100;
    return scaled;
}

// WS2812 byte-level encoder that encodes RGB data into RMT symbols
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    uint32_t bit0_duration_0;
    uint32_t bit0_duration_1;
    uint32_t bit1_duration_0;
    uint32_t bit1_duration_1;
} ws2812_encoder_t;

static size_t ws2812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *data, size_t data_size, rmt_encode_state_t *ret_state) {
    ws2812_encoder_t *ws2812_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ws2812_encoder->bytes_encoder;
    
    // Convert RGB values to required format
    rmt_encode_state_t session_state = {};  // Initialize with zeros
    size_t encoded_symbols = bytes_encoder->encode(bytes_encoder, channel, data, data_size, &session_state);
    
    *ret_state = session_state;
    return encoded_symbols;
}

static esp_err_t ws2812_encoder_reset(rmt_encoder_t *encoder) {
    ws2812_encoder_t *ws2812_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ws2812_encoder->bytes_encoder;
    return bytes_encoder->reset(bytes_encoder);
}

static esp_err_t ws2812_encoder_delete(rmt_encoder_t *encoder) {
    ws2812_encoder_t *ws2812_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ws2812_encoder->bytes_encoder;
    esp_err_t ret = rmt_del_encoder(bytes_encoder);
    free(ws2812_encoder);
    return ret;
}

// Create a custom WS2812 encoder
static esp_err_t create_ws2812_encoder(rmt_encoder_handle_t *ret_encoder) {
    esp_err_t ret = ESP_OK;
    
    // Allocate encoder structure
    ws2812_encoder_t *ws2812_encoder = calloc(1, sizeof(ws2812_encoder_t));
    if (!ws2812_encoder) {
        return ESP_ERR_NO_MEM;
    }
    
    // Configure encoder
    // Calculate duration in clock ticks based on the resolution (10MHz = 0.1μs per tick)
    // 350ns = 3.5 ticks, 900ns = 9 ticks
    
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .duration0 = 4, // 350ns -> ~4 ticks at 10MHz
            .level0 = 1,
            .duration1 = 9, // 900ns -> ~9 ticks at 10MHz
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = 9, // 900ns -> ~9 ticks at 10MHz
            .level0 = 1,
            .duration1 = 4, // 350ns -> ~4 ticks at 10MHz
            .level1 = 0,
        },
        .flags = {
            .msb_first = 1 // WS2812 transmits MSB first
        }
    };
    
    // Create bytes encoder
    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &ws2812_encoder->bytes_encoder);
    if (ret != ESP_OK) {
        free(ws2812_encoder);
        return ret;
    }
    
    ws2812_encoder->base.encode = ws2812_encode;
    ws2812_encoder->base.reset = ws2812_encoder_reset;
    ws2812_encoder->base.del = ws2812_encoder_delete;
    
    *ret_encoder = &ws2812_encoder->base;
    return ESP_OK;
}

esp_err_t neopixel_init(void) {
    ESP_LOGI(TAG, "Initializing NeoPixel");
    
    // RMT TX channel configuration
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = NEOPIXEL_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, // 10MHz (100ns per tick)
        .mem_block_symbols = 64,   // Memory blocks for channel
        .trans_queue_depth = 4,    // TX queue depth
        .flags = {
            .invert_out = false    // No signal inversion
        }
    };
    
    esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT TX channel: %d", ret);
        return ret;
    }
    
    // Create WS2812 encoder
    ret = create_ws2812_encoder(&led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create encoder: %d", ret);
        return ret;
    }
    
    // Enable the RMT TX channel
    ret = rmt_enable(led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %d", ret);
        return ret;
    }
    
    // Turn off the NeoPixel at initialization
    return neopixel_off();
}

esp_err_t neopixel_set_color(rgb_color_t color) {
    // Scale color by brightness
    rgb_color_t scaled_color = neopixel_scale_brightness(color);
    
    // WS2812 expects GRB format, but we need to swap red and green based on observation
    // that "red shows as green and green shows as red"
    uint8_t led_data[3] = {
        scaled_color.red,      // Red (was green in old code)
        scaled_color.green,    // Green (was red in old code) 
        scaled_color.blue      // Blue
    };
    
    // Configuration for sending RMT TX data
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,     // No loop
        .flags = {
            .eot_level = 0   // Set output level to low after transmission
        }
    };
    
    // Send data using RMT
    esp_err_t ret = rmt_transmit(led_chan, led_encoder, led_data, sizeof(led_data), &tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit RMT data: %d", ret);
    }
    
    return ret;
}

esp_err_t neopixel_off(void) {
    rgb_color_t off = {0, 0, 0};
    return neopixel_set_color(off);
}

esp_err_t neopixel_blink(rgb_color_t color, uint32_t duration_ms) {
    esp_err_t ret = neopixel_set_color(color);
    if (ret != ESP_OK) {
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    
    return neopixel_off();
}

esp_err_t neopixel_indicate_mode(playback_mode_t mode) {
    if (mode >= MODE_MAX) {
        ESP_LOGE(TAG, "Invalid mode: %d", mode);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Blink the LED with the color for this mode for 500ms
    ESP_LOGI(TAG, "Indicating mode %d with color", mode);
    return neopixel_blink(mode_colors[mode], 500);
}
