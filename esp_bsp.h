#pragma once

enum {
    SELECT_BUTTON_ID = 0,
    UP_BUTTON_ID,
    DOWN_BUTTON_ID,
    LEFT_BUTTON_ID,
    RIGHT_BUTTON_ID,
    BUTTON_COUNT
};

bool button_is_pressed(uint8_t id);
bool button_is_holding(uint8_t id);
void esp_bsp_loop(void);
void esp_bsp_init(void);
