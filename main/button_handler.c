#include "button_handler.h"
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"

static const char *TAG = "button_handler";

// Button states
#define BTN_STATE_IDLE     0
#define BTN_STATE_PRESSED  1
#define BTN_STATE_RELEASED 2
#define BTN_STATE_LONGPRESS 3

// Button timing
#define BTN_DEBOUNCE_TIME_MS 50
#define BTN_LONGPRESS_TIME_MS 1000 // 1 second for longpress
#define RESTART_TRACK_TIMEOUT_MS 5000 // 5 seconds for restart track vs prev track

// Button structure
typedef struct {
    int pin;
    int state;
    int64_t press_time;
    int64_t release_time;
    bool long_press_detected;
    button_action_t last_action;
} button_t;

// Static button instances
static button_t btn_fwd = {BTN_FWD_PIN, BTN_STATE_IDLE, 0, 0, false, BTN_ACTION_NONE};
static button_t btn_bck = {BTN_BCK_PIN, BTN_STATE_IDLE, 0, 0, false, BTN_ACTION_NONE};
static button_t btn_menu = {BTN_MENU_PIN, BTN_STATE_IDLE, 0, 0, false, BTN_ACTION_NONE};

// Last back button press time to detect restart track or previous track
static int64_t last_back_press_time = 0;

// ISR service handle
static int btn_isr_service = -1;

// Button GPIO interrupt handler
static void IRAM_ATTR gpio_isr_handler(void *arg) {
    button_t *btn = (button_t *)arg;
    int level = gpio_get_level(btn->pin);
    
    int64_t current_time = esp_timer_get_time() / 1000;
    
    if (level == 0) { // Button pressed (active low)
        btn->press_time = current_time;
        btn->state = BTN_STATE_PRESSED;
        btn->long_press_detected = false;
    } else { // Button released
        btn->release_time = current_time;
        if (btn->state == BTN_STATE_PRESSED || btn->state == BTN_STATE_LONGPRESS) {
            btn->state = BTN_STATE_RELEASED;
        }
    }
}

// Initialize button handling
esp_err_t button_handler_init(void) {
    ESP_LOGI(TAG, "Initializing button handler");
    
    // Configure button GPIO pins
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE, // Interrupt on both rising and falling edges
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN_FWD_PIN) | (1ULL << BTN_BCK_PIN) | (1ULL << BTN_MENU_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE, // Enable pull-up resistors
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO pins");
        return ret;
    }
    
    // Install GPIO ISR service
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service");
        return ret;
    }
    
    // Add ISR handlers for buttons
    gpio_isr_handler_add(BTN_FWD_PIN, gpio_isr_handler, (void *)&btn_fwd);
    gpio_isr_handler_add(BTN_BCK_PIN, gpio_isr_handler, (void *)&btn_bck);
    gpio_isr_handler_add(BTN_MENU_PIN, gpio_isr_handler, (void *)&btn_menu);
    
    ESP_LOGI(TAG, "Button handler initialized successfully");
    return ESP_OK;
}

// Process button states and return the appropriate action
button_action_t button_handler_get_action(void) {
    button_action_t action = BTN_ACTION_NONE;
    int64_t current_time = esp_timer_get_time() / 1000;
    
    // Check for longpress on forward button
    if (btn_fwd.state == BTN_STATE_PRESSED && !btn_fwd.long_press_detected &&
        (current_time - btn_fwd.press_time) > BTN_LONGPRESS_TIME_MS) {
        btn_fwd.long_press_detected = true;
        btn_fwd.state = BTN_STATE_LONGPRESS;
        btn_fwd.last_action = BTN_ACTION_NEXT_FOLDER;
        return BTN_ACTION_NEXT_FOLDER;
    }
    
    // Check for longpress on back button
    if (btn_bck.state == BTN_STATE_PRESSED && !btn_bck.long_press_detected &&
        (current_time - btn_bck.press_time) > BTN_LONGPRESS_TIME_MS) {
        btn_bck.long_press_detected = true;
        btn_bck.state = BTN_STATE_LONGPRESS;
        btn_bck.last_action = BTN_ACTION_PREV_FOLDER;
        return BTN_ACTION_PREV_FOLDER;
    }
    
    // Process forward button release
    if (btn_fwd.state == BTN_STATE_RELEASED) {
        int press_duration = btn_fwd.release_time - btn_fwd.press_time;
        
        if (press_duration >= BTN_DEBOUNCE_TIME_MS && 
            press_duration < BTN_LONGPRESS_TIME_MS) {
            // Short press detected
            btn_fwd.last_action = BTN_ACTION_NEXT;
            action = BTN_ACTION_NEXT;
        }
        
        btn_fwd.state = BTN_STATE_IDLE;
    }
    
    // Process back button release
    if (btn_bck.state == BTN_STATE_RELEASED) {
        int press_duration = btn_bck.release_time - btn_bck.press_time;
        
        if (press_duration >= BTN_DEBOUNCE_TIME_MS && 
            press_duration < BTN_LONGPRESS_TIME_MS) {
            // Short press detected
            if ((current_time - last_back_press_time) < RESTART_TRACK_TIMEOUT_MS) {
                btn_bck.last_action = BTN_ACTION_PREV;
                action = BTN_ACTION_PREV;
            } else {
                btn_bck.last_action = BTN_ACTION_RESTART_TRACK;
                action = BTN_ACTION_RESTART_TRACK;
            }
            
            last_back_press_time = current_time;
        }
        
        btn_bck.state = BTN_STATE_IDLE;
    }
    
    // Process menu button release
    if (btn_menu.state == BTN_STATE_RELEASED) {
        int press_duration = btn_menu.release_time - btn_menu.press_time;
        
        if (press_duration >= BTN_DEBOUNCE_TIME_MS) {
            // Menu button press detected
            btn_menu.last_action = BTN_ACTION_CHANGE_MODE;
            action = BTN_ACTION_CHANGE_MODE;
        }
        
        btn_menu.state = BTN_STATE_IDLE;
    }
    
    return action;
}
