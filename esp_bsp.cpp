#include <Arduino.h>
#include "app_config.h"
#include "esp_bsp.h"

#define DEBOUNCE_MS 60
#define HOLDING_MS  900

typedef struct {
    bool pressed;
    bool holding;
    int state;
    int last_state;
    uint32_t last_time;
    uint32_t holding_ms;
} button_state_t;

static const int button_pins[BUTTON_COUNT] = {
    BUTTON_SELECT_PIN,
    BUTTON_UP_PIN,
    BUTTON_DOWN_PIN,
    BUTTON_LEFT_PIN,
    BUTTON_RIGHT_PIN,
};

static button_state_t button_state[BUTTON_COUNT];

static void button_loop(void) {
    int reading;
    uint32_t vol;
    uint32_t elapsed;
    uint32_t now = millis();

    for (int i = 0; i < BUTTON_COUNT; i++) {
        /* Normal button */
        reading = digitalRead(button_pins[i]);

        /* Check if state is changed */
        if (reading != button_state[i].last_state) {
            button_state[i].last_time = now;
        }

        /* Handle after noise */
        if (((now - button_state[i].last_time) & 0xFFFFFFFF) > DEBOUNCE_MS) {
            if (reading != button_state[i].state) {
                button_state[i].state = reading;

                if (button_state[i].state == HIGH) {
                    if (((now - button_state[i].holding_ms) & 0xFFFFFFFF) > HOLDING_MS) {
                        button_state[i].holding = true;
                    }
                    else {
                        button_state[i].pressed = true;
                    }
                }
                else {
                    button_state[i].holding_ms = now;
                }
            }
        } 

        button_state[i].last_state = reading;
    }
}

bool button_is_pressed(uint8_t id) {
    if (button_state[id].pressed) {
        button_state[id].pressed = false;
        return true;
    }
    return false;
}

bool button_is_holding(uint8_t id) {
    if (button_state[id].holding) {
        button_state[id].holding = false;
        return true;
    }
    return false;
}

void esp_bsp_loop(void) {
    button_loop();
}

void esp_bsp_init(void) {
    memset(&button_state, 0, sizeof(button_state));
    for (int i = 0; i < BUTTON_COUNT; i++) {
        pinMode(button_pins[i], INPUT_PULLUP);
        button_state[i].last_time = CURRENT_TIME_MS();
        button_state[i].state = digitalRead(button_pins[i]);
        button_state[i].last_state = button_state[i].state;

        button_state[i].holding_ms = CURRENT_TIME_MS();
        button_state[i].holding = false;
    }

    pinMode(VIBE, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    digitalWrite(VIBE, LOW);
    digitalWrite(RED_LED, LOW);
}
