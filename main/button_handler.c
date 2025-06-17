#include "button_handler.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../components/ezbutton/ezbutton.h"

static const char *TAG = "button_handler";

// Button timing
#define BTN_LONGPRESS_TIME_MS 1000 // 1 second for longpress
#define RESTART_TRACK_TIMEOUT_MS 2000 // 5 seconds for restart track vs prev track

// Button instances
static ezButton_t *btn_fwd = NULL;
static ezButton_t *btn_bck = NULL;
static ezButton_t *btn_menu = NULL;

// Last back button press time to detect restart track or previous track
static unsigned long last_back_press_time = 0;

// Flags to ensure we only process button events once
static bool fwd_processed = false;
static bool bck_processed = false;
static bool menu_processed = false;

// Initialize button handling
esp_err_t button_handler_init(void) {
    ESP_LOGI(TAG, "Initializing button handler");
    
    // Create button instances
    btn_fwd = ezButton_create(BTN_FWD_PIN, EZBUTTON_PULLUP);
    if (btn_fwd == NULL) {
        ESP_LOGE(TAG, "Failed to create forward button");
        return ESP_FAIL;
    }
    
    btn_bck = ezButton_create(BTN_BCK_PIN, EZBUTTON_PULLUP);
    if (btn_bck == NULL) {
        ESP_LOGE(TAG, "Failed to create back button");
        ezButton_delete(btn_fwd);
        return ESP_FAIL;
    }
    
    btn_menu = ezButton_create(BTN_MENU_PIN, EZBUTTON_PULLUP);
    if (btn_menu == NULL) {
        ESP_LOGE(TAG, "Failed to create menu button");
        ezButton_delete(btn_fwd);
        ezButton_delete(btn_bck);
        return ESP_FAIL;
    }
    
    // Configure button debounce times
    ezButton_setDebounceTime(btn_fwd, 50); // 50ms debounce
    ezButton_setDebounceTime(btn_bck, 50); // 50ms debounce
    ezButton_setDebounceTime(btn_menu, 50); // 50ms debounce
    
    // Configure long press times
    ezButton_setLongPressTime(btn_fwd, BTN_LONGPRESS_TIME_MS);
    ezButton_setLongPressTime(btn_bck, BTN_LONGPRESS_TIME_MS);
    
    ESP_LOGI(TAG, "Button handler initialized successfully");
    return ESP_OK;
}

// Process button states and return the appropriate action
button_action_t button_handler_get_action(void) {
    unsigned long current_time = pdTICKS_TO_MS(xTaskGetTickCount());
    
    // MUST call loop() first to update button states
    ezButton_loop(btn_fwd);
    ezButton_loop(btn_bck);
    ezButton_loop(btn_menu);
    
    // Check for button presses - similar to the example
    
    // Forward button handling
    if (ezButton_isPressed(btn_fwd)) {
        // Button was just pressed, not yet processed
        ESP_LOGD(TAG, "Forward button pressed");
        fwd_processed = false;
    }
    
    if (ezButton_isReleased(btn_fwd) && !fwd_processed) {
        // Only process once per press-release cycle
        ESP_LOGD(TAG, "Forward button released");
        fwd_processed = true;
        
        // Check if it was a long press
        if (ezButton_isLongPressed(btn_fwd)) {
            return BTN_ACTION_NEXT_FOLDER;
        } else {
            return BTN_ACTION_NEXT;
        }
    }
    
    // Back button handling
    if (ezButton_isPressed(btn_bck)) {
        ESP_LOGD(TAG, "Back button pressed");
        bck_processed = false;
    }
    
    if (ezButton_isReleased(btn_bck) && !bck_processed) {
        ESP_LOGD(TAG, "Back button released");
        bck_processed = true;
        
        // Check if it was a long press
        if (ezButton_isLongPressed(btn_bck)) {
            return BTN_ACTION_PREV_FOLDER;
        } else {
            // Determine if this is a restart track or previous track action
            if ((current_time - last_back_press_time) < RESTART_TRACK_TIMEOUT_MS) {
                last_back_press_time = current_time;
                return BTN_ACTION_PREV;
            } else {
                last_back_press_time = current_time;
                return BTN_ACTION_RESTART_TRACK;
            }
        }
    }
    
    // Menu button handling
    if (ezButton_isPressed(btn_menu)) {
        ESP_LOGD(TAG, "Menu button pressed");
        menu_processed = false;
    }
    
    if (ezButton_isReleased(btn_menu) && !menu_processed) {
        ESP_LOGD(TAG, "Menu button released");
        menu_processed = true;
        return BTN_ACTION_CHANGE_MODE;
    }
    
    return BTN_ACTION_NONE;
}
