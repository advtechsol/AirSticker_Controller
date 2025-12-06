#pragma once

enum {
    SCREEN_PING = 0,
};

typedef struct {
    uint8_t screen_id;
    uint8_t last_device_count;
    uint32_t last_ping_ms;
} system_status_t;

void display_draw_ping_screen(system_status_t *display);

void display_loop(system_status_t *status, bool force_update = false);
void display_init(void);
