#include "ezbutton.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ezbutton";

ezButton_t* ezButton_create(int pin, int mode) {
    ezButton_t* button = malloc(sizeof(ezButton_t));
    if (!button) {
        ESP_LOGE(TAG, "Failed to allocate memory for button");
        return NULL;
    }
    
    button->pin = pin;
    button->debounceTime = 50; // Default 50ms debounce time
    button->count = 0;
    button->countMode = EZBUTTON_COUNT_FALLING;
    
    // Configure the GPIO
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.mode = GPIO_MODE_INPUT;
    
    if (mode == EZBUTTON_PULLUP) {
        io_conf.pull_up_en = true;
        io_conf.pull_down_en = false;
        button->pressedState = 0;   // Active low
        button->unpressedState = 1; // Pulled up
    } else { // EZBUTTON_PULLDOWN
        io_conf.pull_up_en = false;
        io_conf.pull_down_en = true;
        button->pressedState = 1;   // Active high
        button->unpressedState = 0; // Pulled down
    }
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO pin %d", pin);
        free(button);
        return NULL;
    }
    
    // Initial state
    button->previousSteadyState = gpio_get_level(pin);
    button->lastSteadyState = button->previousSteadyState;
    button->lastFlickerableState = button->previousSteadyState;
    
    button->lastDebounceTime = 0;
    button->longPressTime = 1000;   // Default 1s long press time
    button->isLongDetected = false;
    button->pressStartTime = 0;
    
    return button;
}

void ezButton_delete(ezButton_t* button) {
    if (button) {
        free(button);
    }
}

void ezButton_setDebounceTime(ezButton_t* button, unsigned long time) {
    if (button) {
        button->debounceTime = time;
    }
}

void ezButton_setLongPressTime(ezButton_t* button, unsigned long time) {
    if (button) {
        button->longPressTime = time;
    }
}

int ezButton_getState(ezButton_t* button) {
    if (!button) return -1;
    return button->lastSteadyState;
}

int ezButton_getStateRaw(ezButton_t* button) {
    if (!button) return -1;
    return gpio_get_level(button->pin);
}

bool ezButton_isPressed(ezButton_t* button) {
    if (!button) return false;
    if (button->previousSteadyState == button->unpressedState && 
        button->lastSteadyState == button->pressedState)
        return true;
    else
        return false;
}

bool ezButton_isReleased(ezButton_t* button) {
    if (!button) return false;
    if (button->previousSteadyState == button->pressedState && 
        button->lastSteadyState == button->unpressedState)
        return true;
    else
        return false;
}

bool ezButton_isLongPressed(ezButton_t* button) {
    if (!button) return false;
    
    // If button is currently pressed and we're past the long press time
    if (button->lastSteadyState == button->pressedState) {
        unsigned long currentTime = pdTICKS_TO_MS(xTaskGetTickCount());
        if (!button->isLongDetected && 
            (currentTime - button->pressStartTime) > button->longPressTime) {
            button->isLongDetected = true;
            return true;
        }
    }
    return false;
}

void ezButton_setCountMode(ezButton_t* button, int mode) {
    if (button) {
        button->countMode = mode;
    }
}

unsigned long ezButton_getCount(ezButton_t* button) {
    if (!button) return 0;
    return button->count;
}

void ezButton_resetCount(ezButton_t* button) {
    if (button) {
        button->count = 0;
    }
}

void ezButton_loop(ezButton_t* button) {
    if (!button) return;
    
    // Read the current state of the button
    int currentState = gpio_get_level(button->pin);
    unsigned long currentTime = pdTICKS_TO_MS(xTaskGetTickCount());
    
    // If the switch/button changed, due to noise or pressing
    if (currentState != button->lastFlickerableState) {
        // Reset the debouncing timer
        button->lastDebounceTime = currentTime;
        // Save the flickerable state
        button->lastFlickerableState = currentState;
    }
    
    // If debounce time has passed, accept the button state as steady
    if ((currentTime - button->lastDebounceTime) >= button->debounceTime) {
        // If the button state has changed
        if (button->lastSteadyState != currentState) {
            button->previousSteadyState = button->lastSteadyState;
            button->lastSteadyState = currentState;
            
            // Update button count based on mode
            if (button->countMode == EZBUTTON_COUNT_BOTH) {
                button->count++;
            } else if (button->countMode == EZBUTTON_COUNT_FALLING) {
                if (button->previousSteadyState == button->unpressedState && 
                    button->lastSteadyState == button->pressedState)
                    button->count++;
            } else if (button->countMode == EZBUTTON_COUNT_RISING) {
                if (button->previousSteadyState == button->pressedState && 
                    button->lastSteadyState == button->unpressedState)
                    button->count++;
            }
            
            // Track press start time
            if (button->lastSteadyState == button->pressedState) {
                button->pressStartTime = currentTime;
                button->isLongDetected = false;
            }
        }
    }
}
