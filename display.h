#pragma once

#include "bluetooth.h"

#define MAX_LINES 5
#define MAX_ACTION 3

enum {
    SCREEN_PING = 0,
    SCREEN_SCANNING,
    SCREEN_DEVICE_LIST,
    SCREEN_ACTIONS,
    SCREEN_COUNT,
};

#define SCREEN_NAMES {        \
        "SCREEN_PING",        \
        "SCREEN_SCANNING",    \
        "SCREEN_DEVICE_LIST", \
        "SCREEN_ACTIONS",     \
    }

typedef struct {
    bool force_update;
    uint8_t screen_id;
    uint8_t device_count;
    uint8_t last_device_count;
    uint8_t selected_device;
    uint8_t selected_index;
    uint32_t start_scanning_ms;
    uint32_t last_ping_ms;
    char names[MAX_LINES][BLE_NAME_MAX_LEN + 16];
    tag_t selected_tag;
} system_status_t;

void display_loop(system_status_t *status);
void display_init(void);
