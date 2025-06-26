#include "button_handler.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../components/ezbutton/ezbutton.h"
#include "audio_player.h"

static const char *TAG = "button_handler";

#define BTN_LONGPRESS_TIME_MS 1000
#define RESTART_TRACK_TIMEOUT_MS 2000

static ezButton_t *btn_fwd = NULL;
static ezButton_t *btn_bck = NULL;
static ezButton_t *btn_menu = NULL;

// State machine for each button
typedef struct {
    bool last_state;
    unsigned long pressed_time;
    bool long_press_handled;
} button_state_t;

static button_state_t fwd_state = {0};
static button_state_t bck_state = {0};
static button_state_t menu_state = {0};

static unsigned long last_back_press_time = 0;

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
    
    ESP_LOGI(TAG, "Button handler initialized successfully");
    return ESP_OK;
}

// Process button states and return the appropriate action
button_action_t button_handler_get_action(void) {
    unsigned long now = pdTICKS_TO_MS(xTaskGetTickCount());
    
    // MUST call loop() first to update button states
    ezButton_loop(btn_fwd);
    ezButton_loop(btn_bck);
    ezButton_loop(btn_menu);
    
    player_state_t state = audio_player_get_state();

    // FORWARD BUTTON
    bool fwd_pressed = ezButton_getState(btn_fwd);
    if (fwd_pressed && !fwd_state.last_state) {
        fwd_state.pressed_time = now;
        fwd_state.long_press_handled = false;
    }
    if (!fwd_pressed && fwd_state.last_state) {
        unsigned long press_duration = now - fwd_state.pressed_time;
        if ((state.mode == MODE_PLAY_FOLDER_ORDER || state.mode == MODE_PLAY_FOLDER_SHUFFLE) && press_duration >= BTN_LONGPRESS_TIME_MS) {
            return BTN_ACTION_NEXT_FOLDER;
        } else if (press_duration < BTN_LONGPRESS_TIME_MS) {
            return BTN_ACTION_NEXT;
        }
    }
    fwd_state.last_state = fwd_pressed;

    // BACK BUTTON
    bool bck_pressed = ezButton_getState(btn_bck);
    if (bck_pressed && !bck_state.last_state) {
        bck_state.pressed_time = now;
        bck_state.long_press_handled = false;
    }
    if (!bck_pressed && bck_state.last_state) {
        unsigned long press_duration = now - bck_state.pressed_time;
        if ((state.mode == MODE_PLAY_FOLDER_ORDER || state.mode == MODE_PLAY_FOLDER_SHUFFLE) && press_duration >= BTN_LONGPRESS_TIME_MS) {
            return BTN_ACTION_PREV_FOLDER;
        } else if (press_duration < BTN_LONGPRESS_TIME_MS) {
            // Short press: restart or prev track
            if ((now - last_back_press_time) < RESTART_TRACK_TIMEOUT_MS) {
                last_back_press_time = now;
                return BTN_ACTION_PREV;
            } else {
                last_back_press_time = now;
                return BTN_ACTION_RESTART_TRACK;
            }
        }
    }
    bck_state.last_state = bck_pressed;

    // MENU BUTTON
    bool menu_pressed = ezButton_getState(btn_menu);
    if (menu_pressed && !menu_state.last_state) {
        menu_state.pressed_time = now;
        menu_state.long_press_handled = false;
    }
    if (!menu_pressed && menu_state.last_state) {
        // Only short press for menu
        return BTN_ACTION_CHANGE_MODE;
    }
    menu_state.last_state = menu_pressed;

    return BTN_ACTION_NONE;
}
