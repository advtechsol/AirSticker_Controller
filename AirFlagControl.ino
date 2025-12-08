#include <Arduino.h>
#include "app_config.h"
#include "esp_bsp.h"
#include "display.h"
#include "bluetooth.h"

typedef void (*button_handler_t)(void);

static const char *screen_names[] = SCREEN_NAMES;
static tag_scan_t *tags;

system_status_t system_status;

/******************************************************************************/

static void fill_devices_name(void) {
    uint8_t start = 0;
    uint8_t line = 0;

    bool list_short = (system_status.device_count + 1 <= MAX_LINES);
    uint8_t max_devices_to_show = list_short ? system_status.device_count : MAX_LINES - 1;

    if (!list_short) {
        if (system_status.selected_device >= max_devices_to_show) {
            start = system_status.selected_device - (max_devices_to_show - 1);
            if (start > system_status.device_count - max_devices_to_show) {
                start = system_status.device_count - max_devices_to_show;
            }
        }
    }

    for (uint8_t line = 0; line < MAX_LINES; line++) {
        snprintf(system_status.names[line], sizeof(system_status.names[line]), " ");
    }

    for (uint8_t i = start; i < system_status.device_count && line < max_devices_to_show; i++) {
        if (i == system_status.selected_device)
            system_status.selected_index = line;

        snprintf(system_status.names[line], sizeof(system_status.names[line]),
                (i + 1 > 9) ? "> %d. %s" : ">  %d. %s",
                i + 1, tags->tags[i].name);
        line++;
    }

    snprintf(system_status.names[line], sizeof(system_status.names[line]), "  [ Back ]");
    if (system_status.selected_device == system_status.device_count) {
        system_status.selected_index = line;
    }
}

/******************************************************************************/

static void handle_select(void) {
    switch (system_status.screen_id) {
        case SCREEN_PING:
            system_status.screen_id = SCREEN_SCANNING;
            system_status.last_ping_ms = CURRENT_TIME_MS();
            bluetooth_start_scanning();
            break;

        case SCREEN_DEVICE_LIST:
            if (system_status.device_count > 0) {
                if (system_status.selected_device >= system_status.device_count) {
                    /* Back to ping */
                    system_status.screen_id = SCREEN_PING;
                }
                else {
                    LOG_PRINTLN(system_status.names[system_status.selected_index]);
                    tags = bluetooth_get_tag_list();
                    memcpy(&system_status.selected_tag, &tags->tags[system_status.selected_device], sizeof(tag_t));
                    bluetooth_release_tag_list();
                    system_status.selected_index = 0;
                    system_status.screen_id = SCREEN_ACTIONS;
                }
            }
            break;

        case SCREEN_ACTIONS:
            if (system_status.selected_index >= MAX_ACTION - 1) {
                /* Back to device list screen */
                system_status.screen_id = SCREEN_DEVICE_LIST;
                fill_devices_name();
            }
            else {
                LOG_PRINTF("Action %d\n", system_status.selected_index);
            }
            break;

        default:
            break;
    }
}

static void handle_up(void) {
    switch (system_status.screen_id) {
        case SCREEN_DEVICE_LIST:
            if (system_status.selected_device > 0) {
                system_status.selected_device--;
                fill_devices_name();
            }
            break;

        case SCREEN_ACTIONS:
            if (system_status.selected_index) {
                system_status.selected_index--;
            }
            break;

        default:
            break;
    }
}

static void handle_down(void) {
    switch (system_status.screen_id) {
        case SCREEN_DEVICE_LIST:
            if (system_status.selected_device < system_status.device_count) {
                system_status.selected_device++;
                fill_devices_name();
            }
            break;

        case SCREEN_ACTIONS:
            if (system_status.selected_index < MAX_ACTION - 1) {
                system_status.selected_index++;
            }
            break;

        default:
            break;
    }
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
            system_status.force_update = true;
        }
    }

    if (button_is_holding(SELECT_BUTTON_ID)) {
        handle_select_holding();
        system_status.force_update = true;
    }
}

static void machine_state(void) {
    static uint8_t last_screen_id = SCREEN_COUNT;
    if (system_status.screen_id != last_screen_id) {
        if ((last_screen_id < SCREEN_COUNT) && (system_status.screen_id < SCREEN_COUNT)) {
            LOG_PRINTF("Change screen from %s to %s\n", screen_names[last_screen_id], screen_names[system_status.screen_id]);
        }
        last_screen_id = system_status.screen_id;
    }

    switch (system_status.screen_id) {
        case SCREEN_PING:
            system_status.start_scanning_ms = CURRENT_TIME_MS();
            break;

        case SCREEN_SCANNING: {
            tags = bluetooth_get_tag_list();
            system_status.device_count = tags->count;

            if (ELAPSED_TIME_MS(system_status.start_scanning_ms) > GAP_SCAN_DURATION * 1000) {
                if (system_status.device_count > 0) { 
                    system_status.screen_id = SCREEN_DEVICE_LIST;

                    system_status.selected_device = 0;
                    system_status.last_device_count = system_status.device_count;
                    fill_devices_name();
                }
                else {
                    LOG_PRINTLN("No device found");
                    system_status.screen_id = SCREEN_PING;
                }
            }

            bluetooth_release_tag_list();
            break;
        }

        case SCREEN_DEVICE_LIST:
            system_status.last_device_count = system_status.device_count;
            break;

        default:
            break;
    }
}

/******************************************************************************/

void setup() {
    LOG_BEGIN(115200);
    memset(&system_status, 0, sizeof(system_status));

    esp_bsp_init();
    display_init();
    bluetooth_init();
    
    LOG_PRINTLN("Start main loop");
}

void loop() {
    esp_bsp_loop();
    user_inft_loop();
    display_loop(&system_status);
    machine_state();
}
