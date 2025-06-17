#include "neopixel.h"
#include <stdlib.h>
#include <string.h>
#include "driver/rmt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "neopixel";

// NeoPixel configuration
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_CLK_DIV 8    // RMT clock divider

// Calculate RMT tick value for 10ns (ensure we never divide by zero)
// ESP32 base clock is 80MHz and after dividing by RMT_CLK_DIV (8) we get 10MHz
// So one tick is 100ns, and 10ns is 0.1 ticks
#define RMT_TICK_10_NS 0.1f   // Each tick is 100ns, so 10ns is 0.1 ticks

#define WS2812_T0H_NS 350                                  // 0 bit high time
#define WS2812_T0L_NS 900                                  // 0 bit low time
#define WS2812_T1H_NS 900                                  // 1 bit high time
#define WS2812_T1L_NS 350                                  // 1 bit low time

// Derived timing parameters (using direct calculation to avoid division by small values)
#define WS2812_T0H_TICKS 4   // (350ns / 100ns per tick = ~4 ticks)
#define WS2812_T0L_TICKS 9   // (900ns / 100ns per tick = ~9 ticks)
#define WS2812_T1H_TICKS 9   // (900ns / 100ns per tick = ~9 ticks)
#define WS2812_T1L_TICKS 4   // (350ns / 100ns per tick = ~4 ticks)

// Mode colors
static const rgb_color_t mode_colors[MODE_MAX] = {
    {255, 0, 0},     // Red - MODE_PLAY_ALL_ORDER
    {0, 255, 0},     // Green - MODE_PLAY_ALL_SHUFFLE
    {0, 0, 255},     // Blue - MODE_PLAY_FOLDER_ORDER
    {255, 255, 0}    // Yellow - MODE_PLAY_FOLDER_SHUFFLE
};

// Function to convert RGB values to NeoPixel RMT format
static void neopixel_set_pixel(rmt_item32_t *items, rgb_color_t color) {
    uint32_t bits = (color.green << 16) | (color.red << 8) | color.blue; // GRB format for WS2812
    
    for (int i = 0; i < 24; i++) {
        if (bits & (1 << (23 - i))) {
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T1H_TICKS;
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T1L_TICKS;
        } else {
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T0H_TICKS;
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T0L_TICKS;
        }
    }
    
    // Reset code - not needed as rmt_write_items will append it
}

esp_err_t neopixel_init(void) {
    ESP_LOGI(TAG, "Initializing NeoPixel");
    
    // RMT configuration
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_TX_CHANNEL,
        .gpio_num = NEOPIXEL_PIN,
        .clk_div = RMT_CLK_DIV,
        .mem_block_num = 1,
        .tx_config = {
            .loop_en = false,
            .carrier_en = false,
            .idle_output_en = true,
            .idle_level = 0
        }
    };
    
    esp_err_t ret = rmt_config(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure RMT");
        return ret;
    }
    
    ret = rmt_driver_install(config.channel, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install RMT driver");
        return ret;
    }
    
    // Turn off the NeoPixel at initialization
    return neopixel_off();
}

esp_err_t neopixel_set_color(rgb_color_t color) {
    // Create RMT items
    rmt_item32_t items[24];
    neopixel_set_pixel(items, color);
    
    // Send the items to RMT TX channel
    esp_err_t ret = rmt_write_items(RMT_TX_CHANNEL, items, 24, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send RMT items");
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
