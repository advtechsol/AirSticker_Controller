#include <Arduino.h>
#include "app_config.h"
#include "esp_bsp.h"
#include "display.h"
#include "bluetooth.h"

typedef void (*button_handler_t)(void);

system_status_t system_status;

/******************************************************************************/

static void handle_select(void) {
    LOG_PRINTLN("BUTTON_SELECT");
}

static void handle_up(void) {
    LOG_PRINTLN("BUTTON_UP");
}

static void handle_down(void) {
    LOG_PRINTLN("BUTTON_DOWN");
}

static void handle_left(void) {
    LOG_PRINTLN("BUTTON_LEFT");
}

static void handle_right(void) {
    LOG_PRINTLN("BUTTON_RIGHT");
}

static void handle_select_holding(void) {
    LOG_PRINTLN("BUTTON_SELECT HOLDING");
}

static const button_handler_t button_handlers[BUTTON_COUNT] = {
    handle_select,
    handle_up,
    handle_down,
    handle_left,
    handle_right,
};

static void user_inft_loop(void) {
    for (int id = 0; id < BUTTON_COUNT; id++) {
        if (button_is_pressed(id)) {
            button_handlers[id]();
        }
    }

    if (button_is_holding(SELECT_BUTTON_ID)) {
        handle_select_holding();
    }
}

/******************************************************************************/

void setup() {
    LOG_BEGIN(115200);

    esp_bsp_init();
    display_init();
    bluetooth_init();
    bluetooth_start_scanning();
    
    LOG_PRINTLN("Start main loop");
}

void loop() {
    esp_bsp_loop();
    user_inft_loop();
    display_loop(&system_status);
}
