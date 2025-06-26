#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

// --- Mocks and testable logic ---

typedef enum {
    BTN_ACTION_NONE = 0,
    BTN_ACTION_NEXT,
    BTN_ACTION_PREV,
    BTN_ACTION_RESTART_TRACK,
    BTN_ACTION_CHANGE_MODE,
    BTN_ACTION_NEXT_FOLDER,
    BTN_ACTION_PREV_FOLDER
} button_action_t;

typedef enum {
    MODE_PLAY_ALL_ORDER = 0,
    MODE_PLAY_ALL_SHUFFLE,
    MODE_PLAY_FOLDER_ORDER,
    MODE_PLAY_FOLDER_SHUFFLE
} player_mode_t;

typedef struct {
    player_mode_t mode;
} player_state_t;

// Testable state machine struct
typedef struct {
    bool last_state;
    unsigned long pressed_time;
    bool long_press_handled;
} button_state_t;

#define BTN_LONGPRESS_TIME_MS 1000
#define RESTART_TRACK_TIMEOUT_MS 2000

static button_state_t fwd_state = {0};
static button_state_t bck_state = {0};
static button_state_t menu_state = {0};
static unsigned long last_back_press_time = 0;

// Testable function (extracted from your handler)
button_action_t test_button_handler_get_action(
    bool fwd_pressed, bool bck_pressed, bool menu_pressed,
    unsigned long now, player_state_t state,
    bool reset_states
) {
    if (reset_states) {
        fwd_state = (button_state_t){0};
        bck_state = (button_state_t){0};
        menu_state = (button_state_t){0};
        last_back_press_time = 0;
    }
    // FORWARD BUTTON
    if (fwd_pressed && !fwd_state.last_state) {
        fwd_state.pressed_time = now;
        fwd_state.long_press_handled = false;
    }
    if (!fwd_pressed && fwd_state.last_state) {
        unsigned long press_duration = now - fwd_state.pressed_time;
        if ((state.mode == MODE_PLAY_FOLDER_ORDER || state.mode == MODE_PLAY_FOLDER_SHUFFLE) && press_duration >= BTN_LONGPRESS_TIME_MS) {
            fwd_state.last_state = fwd_pressed;
            return BTN_ACTION_NEXT_FOLDER;
        } else if (press_duration < BTN_LONGPRESS_TIME_MS) {
            fwd_state.last_state = fwd_pressed;
            return BTN_ACTION_NEXT;
        }
    }
    fwd_state.last_state = fwd_pressed;

    // BACK BUTTON
    if (bck_pressed && !bck_state.last_state) {
        bck_state.pressed_time = now;
        bck_state.long_press_handled = false;
    }
    if (!bck_pressed && bck_state.last_state) {
        unsigned long press_duration = now - bck_state.pressed_time;
        if ((state.mode == MODE_PLAY_FOLDER_ORDER || state.mode == MODE_PLAY_FOLDER_SHUFFLE) && press_duration >= BTN_LONGPRESS_TIME_MS) {
            bck_state.last_state = bck_pressed;
            return BTN_ACTION_PREV_FOLDER;
        } else if (press_duration < BTN_LONGPRESS_TIME_MS) {
            if ((now - last_back_press_time) < RESTART_TRACK_TIMEOUT_MS) {
                last_back_press_time = now;
                bck_state.last_state = bck_pressed;
                return BTN_ACTION_PREV;
            } else {
                last_back_press_time = now;
                bck_state.last_state = bck_pressed;
                return BTN_ACTION_RESTART_TRACK;
            }
        }
    }
    bck_state.last_state = bck_pressed;

    // MENU BUTTON
    if (menu_pressed && !menu_state.last_state) {
        menu_state.pressed_time = now;
        menu_state.long_press_handled = false;
    }
    if (!menu_pressed && menu_state.last_state) {
        menu_state.last_state = menu_pressed;
        return BTN_ACTION_CHANGE_MODE;
    }
    menu_state.last_state = menu_pressed;

    return BTN_ACTION_NONE;
}

// --- Unit tests ---
void test_short_press_next() {
    player_state_t state = { .mode = MODE_PLAY_ALL_ORDER };
    unsigned long t = 1000;
    // Press
    assert(test_button_handler_get_action(true, false, false, t, state, true) == BTN_ACTION_NONE);
    // Release after 100ms
    assert(test_button_handler_get_action(false, false, false, t+100, state, false) == BTN_ACTION_NEXT);
}

void test_long_press_next_folder() {
    player_state_t state = { .mode = MODE_PLAY_FOLDER_ORDER };
    unsigned long t = 2000;
    // Press
    assert(test_button_handler_get_action(true, false, false, t, state, true) == BTN_ACTION_NONE);
    // Release after 1200ms
    assert(test_button_handler_get_action(false, false, false, t+1200, state, false) == BTN_ACTION_NEXT_FOLDER);
}

void test_short_press_prev_restart_logic() {
    player_state_t state = { .mode = MODE_PLAY_ALL_ORDER };
    unsigned long t = 1000;
    // First press/release: should go to prev (since last_back_press_time is 0, so t - 0 < 2000)
    assert(test_button_handler_get_action(false, true, false, t, state, true) == BTN_ACTION_NONE); // press (bck)
    button_action_t act1 = test_button_handler_get_action(false, false, false, t+100, state, false); // release
    printf("First back release action: %d\n", act1);
    assert(act1 == BTN_ACTION_PREV);
    // Second quick press/release: should go to prev
    assert(test_button_handler_get_action(false, true, false, t+200, state, false) == BTN_ACTION_NONE); // press (bck)
    assert(test_button_handler_get_action(false, false, false, t+300, state, false) == BTN_ACTION_PREV); // release
    // After a long interval: should restart track
    assert(test_button_handler_get_action(false, true, false, t+3000, state, false) == BTN_ACTION_NONE); // press (bck)
    assert(test_button_handler_get_action(false, false, false, t+3100, state, false) == BTN_ACTION_RESTART_TRACK); // release
}

void test_menu_press() {
    player_state_t state = { .mode = MODE_PLAY_ALL_ORDER };
    unsigned long t = 5000;
    assert(test_button_handler_get_action(false, false, true, t, state, true) == BTN_ACTION_NONE);
    assert(test_button_handler_get_action(false, false, false, t+50, state, false) == BTN_ACTION_CHANGE_MODE);
}

void test_long_press_prev_folder() {
    player_state_t state = { .mode = MODE_PLAY_FOLDER_SHUFFLE };
    unsigned long t = 3000;
    assert(test_button_handler_get_action(false, true, false, t, state, true) == BTN_ACTION_NONE);
    assert(test_button_handler_get_action(false, false, false, t+1500, state, false) == BTN_ACTION_PREV_FOLDER);
}

int main() {
    test_short_press_next();
    test_long_press_next_folder();
    test_short_press_prev_restart_logic();
    test_menu_press();
    test_long_press_prev_folder();
    printf("All button handler tests passed!\n");
    return 0;
}
