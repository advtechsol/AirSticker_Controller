#pragma once

#include "bluetooth.h"

#define MAX_LINES      5

enum {
    ERROR_NONE = 0,
    ERROR_BLE_CONNECT,
    ERROR_BLE_SEND,
    ERROR_BLE_READ,
    ERROR_COUNT,
};

enum {
    ACTION_CONTROL_GPIO = 0,
    ACTION_CONTROL_BLE,
    ACTION_MENU_BACK,
    ACTION_MENU_COUNT,
};

enum {
    GPIO_CONTROL_OFF = 0,
    GPIO_CONTROL_ON,
    GPIO_MENU_BACK,
    GPIO_MENU_COUNT,
};

enum {
    BLE_CONTROL_OFF = 0,
    BLE_CONTROL_ON,
    BLE_CONTROL_DELAY,
    BLE_MENU_BACK,
    BLE_MENU_COUNT,
};

enum {
    DELAY_CONTROL_0 = 0,
    DELAY_CONTROL_1,
    DELAY_CONTROL_2,
    DELAY_CONTROL_3,
    DELAY_CONTROL_OK,
    DELAY_MENU_BACK,
    DELAY_MENU_COUNT,
};

enum {
    SCREEN_PING = 0,
    SCREEN_SCANNING,
    SCREEN_DEVICE_LIST,
    SCREEN_ACTIONS,
    SCREEN_CONTROL_GPIO,
    SCREEN_CONTROL_BLE,
    SCREEN_SET_DELAY,
    SCREEN_BLE_ERROR,
    SCREEN_COUNT,
};

#define SCREEN_NAMES {         \
        "SCREEN_PING",         \
        "SCREEN_SCANNING",     \
        "SCREEN_DEVICE_LIST",  \
        "SCREEN_ACTIONS",      \
        "SCREEN_CONTROL_GPIO", \
        "SCREEN_CONTROL_BLE",  \
        "SCREEN_SET_DELAY",    \
        "SCREEN_BLE_ERROR",    \
    }

typedef struct {
    bool force_update;
    uint8_t screen_id;
    uint8_t device_count;
    uint8_t last_device_count;
    uint8_t selected_device;
    uint8_t selected_index;
    uint8_t max_index;
    uint8_t error;

    uint8_t gpio_state;      /* From remote device */
    int ble_delay;           /* From remote device (in minutes) */
    int set_ble_delay;       /* Set to remote device (in minutes) */

    uint32_t start_scanning_ms;
    uint32_t last_ping_ms;

    char names[MAX_LINES][BLE_NAME_MAX_LEN + 16];
    tag_t selected_tag;
} system_status_t;

void display_loop(system_status_t *status);
void display_init(void);
