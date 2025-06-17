#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "esp_err.h"

// Button pins
#define BTN_FWD_PIN     33
#define BTN_BCK_PIN     27
#define BTN_MENU_PIN    22

// Button actions
typedef enum {
    BTN_ACTION_NONE = 0,
    BTN_ACTION_NEXT,
    BTN_ACTION_PREV,
    BTN_ACTION_RESTART_TRACK,
    BTN_ACTION_CHANGE_MODE,
    BTN_ACTION_NEXT_FOLDER,
    BTN_ACTION_PREV_FOLDER
} button_action_t;

/**
 * @brief Initialize button handler
 * 
 * @return ESP_OK on success
 */
esp_err_t button_handler_init(void);

/**
 * @brief Get the current button action
 * 
 * @return Button action
 */
button_action_t button_handler_get_action(void);

#endif // BUTTON_HANDLER_H
