#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "sd_card.h"
#include "audio_player.h"
#include "button_handler.h"
#include "neopixel.h"
#include "esp_wifi.h"


static const char *TAG = "main";

// Button polling task
static void button_task(void *arg)
{
    ESP_LOGI(TAG, "Button task started");

    while (1)
    {
        // Check for button actions
        button_action_t action = button_handler_get_action();

        switch (action)
        {
        case BTN_ACTION_NEXT:
            ESP_LOGI(TAG, "Next button pressed");
            audio_player_next();
            break;

        case BTN_ACTION_PREV:
            ESP_LOGI(TAG, "Previous button pressed");
            audio_player_prev();
            break;

        case BTN_ACTION_RESTART_TRACK:
            ESP_LOGI(TAG, "Restart track button pressed");
            // Seek to the beginning of the current file
            player_state_t state_restart = audio_player_get_state();
            if (strlen(state_restart.current_file_path) > 0)
            {
                audio_player_seek(0); // Seek to start of file
            }
            break;

        case BTN_ACTION_CHANGE_MODE:
            ESP_LOGI(TAG, "Mode button pressed");
            // Just cycle to next mode
            player_state_t state_mode = audio_player_get_state();
            audio_player_set_mode((state_mode.mode + 1) % MODE_MAX);
            break;

        case BTN_ACTION_NEXT_FOLDER:
            ESP_LOGI(TAG, "Next folder button pressed");
            audio_player_next_folder();
            break;

        case BTN_ACTION_PREV_FOLDER:
            ESP_LOGI(TAG, "Previous folder button pressed");
            audio_player_prev_folder();
            break;

        case BTN_ACTION_NONE:
        default:
            break;
        }

        // Small delay to prevent CPU hogging - keeping this short for responsive button handling
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Project 3 player starting");

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WiFi stop failed: %s", esp_err_to_name(err));
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WiFi deinit failed: %s", esp_err_to_name(err));
    }

    // Initialize NVS flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SD card
    ret = sd_card_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SD card");
        return;
    }

    // Initialize NeoPixel
    ret = neopixel_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NeoPixel");
    }

    // Initialize button handler
    ret = button_handler_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize button handler");
    }

    // Initialize audio player
    ret = audio_player_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize audio player");
    }
    else
    {
        // Start playback
        audio_player_start();

        // Indicate current mode
        player_state_t state = audio_player_get_state();
        neopixel_indicate_mode(state.mode);
    }

    // Create button polling task with larger stack size to prevent overflow
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Initialization complete");
}
