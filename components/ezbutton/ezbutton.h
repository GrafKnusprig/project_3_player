#ifndef EZBUTTON_H
#define EZBUTTON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_system.h"

// Button count modes
#define EZBUTTON_COUNT_FALLING 0
#define EZBUTTON_COUNT_RISING  1
#define EZBUTTON_COUNT_BOTH    2

// Button modes (pull-up or pull-down)
#define EZBUTTON_PULLUP   1
#define EZBUTTON_PULLDOWN 2

/**
 * ezButton structure for ESP32
 */
typedef struct {
    int pin;                  // GPIO pin number
    unsigned long debounceTime;  // Debounce time in milliseconds
    unsigned long longPressTime; // Long press time in milliseconds
    unsigned long count;      // Press count
    int countMode;            // Count mode
    int pressedState;         // State considered as pressed
    int unpressedState;       // State considered as unpressed
    
    int previousSteadyState;  // Previous steady state
    int lastSteadyState;      // Last steady state
    int lastFlickerableState; // Last flickerable state
    
    unsigned long lastDebounceTime; // Last time the button was debounced
    unsigned long pressStartTime;   // Time when button was pressed
    bool isLongDetected;      // Flag to track if long press was detected
} ezButton_t;

/**
 * Create a new ezButton instance
 * 
 * @param pin GPIO pin number
 * @param mode EZBUTTON_PULLUP or EZBUTTON_PULLDOWN
 * @return Pointer to the new button instance or NULL on failure
 */
ezButton_t* ezButton_create(int pin, int mode);

/**
 * Delete an ezButton instance
 * 
 * @param button Pointer to the button instance
 */
void ezButton_delete(ezButton_t* button);

/**
 * Set the debounce time (default is 50ms)
 * 
 * @param button Pointer to the button instance
 * @param time Debounce time in milliseconds
 */
void ezButton_setDebounceTime(ezButton_t* button, unsigned long time);

/**
 * Set the long press time (default is 1000ms)
 * 
 * @param button Pointer to the button instance
 * @param time Long press time in milliseconds
 */
void ezButton_setLongPressTime(ezButton_t* button, unsigned long time);

/**
 * Get the current debounced state of the button
 * 
 * @param button Pointer to the button instance
 * @return Current state (pressed or unpressed)
 */
int ezButton_getState(ezButton_t* button);

/**
 * Get the current raw state of the button without debouncing
 * 
 * @param button Pointer to the button instance
 * @return Current raw state
 */
int ezButton_getStateRaw(ezButton_t* button);

/**
 * Check if the button was just pressed
 * 
 * @param button Pointer to the button instance
 * @return true if the button was just pressed
 */
bool ezButton_isPressed(ezButton_t* button);

/**
 * Check if the button was just released
 * 
 * @param button Pointer to the button instance
 * @return true if the button was just released
 */
bool ezButton_isReleased(ezButton_t* button);

/**
 * Check if the button is long pressed
 * 
 * @param button Pointer to the button instance
 * @return true if the button is long pressed
 */
bool ezButton_isLongPressed(ezButton_t* button);

/**
 * Check if the button was long pressed since the last call (latches and clears the flag)
 *
 * @param button Pointer to the button instance
 * @return true if a long press event occurred since last call
 */
bool ezButton_wasLongPressed(ezButton_t* button);

/**
 * Set the counting mode for button presses
 * 
 * @param button Pointer to the button instance
 * @param mode EZBUTTON_COUNT_FALLING, EZBUTTON_COUNT_RISING, or EZBUTTON_COUNT_BOTH
 */
void ezButton_setCountMode(ezButton_t* button, int mode);

/**
 * Get the press count
 * 
 * @param button Pointer to the button instance
 * @return Number of times the button was pressed
 */
unsigned long ezButton_getCount(ezButton_t* button);

/**
 * Reset the press count
 * 
 * @param button Pointer to the button instance
 */
void ezButton_resetCount(ezButton_t* button);

/**
 * Update the button state (call this in your loop)
 * 
 * @param button Pointer to the button instance
 */
void ezButton_loop(ezButton_t* button);

#endif /* EZBUTTON_H */
