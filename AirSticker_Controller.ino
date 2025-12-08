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
                (i + 1 > 9) ? "  %d. %s" : "  %d. %s",
                i + 1, tags->tags[i].name);
        line++;
    }

    snprintf(system_status.names[line], sizeof(system_status.names[line]), "  [ Back ]");
    if (system_status.selected_device == system_status.device_count) {
        system_status.selected_index = line;
    }
}

int mod_wrap(int value, int delta, int mod) {
    int n = (value + delta) % mod;
    if (n < 0) {
        n += mod;
    }
    return n;
}

static void adjust_delay_time(int delta) {
    int tensH = (system_status.set_ble_delay / 60) / 10;
    int onesH = (system_status.set_ble_delay / 60) % 10;
    int tensM = (system_status.set_ble_delay % 60) / 10;
    int onesM = (system_status.set_ble_delay % 60) % 10;

    switch (system_status.selected_index) {
        case 0: {
            tensH = mod_wrap(tensH, delta, 3);
            if (tensH == 2 && onesH > 3) onesH = 3;
            break;
        }
        case 1: {
            int maxOnesH = (tensH == 2) ? 3 : 9;
            onesH = mod_wrap(onesH, delta, maxOnesH + 1);
            break;
        }
        case 2: {
            tensM = mod_wrap(tensM, delta, 6);
            break;
        }
        case 3: {
            onesM = mod_wrap(onesM, delta, 10);
            break;
        }
        default:
            break;
    }

    uint8_t delay_hours = tensH * 10 + onesH;
    if (delay_hours > 23) {
        delay_hours = 23;
    }
    
    uint8_t delay_minutes = tensM * 10 + onesM;
    if (delay_minutes > 59) {
        delay_minutes = 59;
    }
    system_status.set_ble_delay = delay_hours * 60 + delay_minutes;
}

/******************************************************************************/

static void enter_error_screen(uint8_t error) {
    LOG_PRINTF("!!! ERROR %d !!!\n", error);
    system_status.error = error;
    system_status.selected_index = 0;
    system_status.max_index = 1;  /* Only Back option */
    system_status.screen_id = SCREEN_BLE_ERROR;
}

static bool main_send_command(const char *cmd) {
    if (!bluetooth_send_command(cmd)) {
        enter_error_screen(ERROR_BLE_SEND);
        return false;
    }
    return true;
}

static void increase_selection(void) {
    if (system_status.screen_id == SCREEN_DEVICE_LIST) {
        if (system_status.selected_device < system_status.device_count) {
            system_status.selected_device++;
            fill_devices_name();
        }
    }
    else {
        if (system_status.selected_index < system_status.max_index - 1) {
            system_status.selected_index++;
        }
        else {
            system_status.selected_index = 0;
        }
    }
}

static void decrease_selection(void) {
    if (system_status.screen_id == SCREEN_DEVICE_LIST) {
        if (system_status.selected_device > 0) {
            system_status.selected_device--;
            fill_devices_name();
        }
    }
    else {
        if (system_status.selected_index) {
            system_status.selected_index--;
        }
        else {
            if (system_status.max_index > 0) {
                system_status.selected_index = system_status.max_index - 1;
            }
        }
    }
}

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
                    bluetooth_airtag_connect(system_status.selected_tag.bda, system_status.selected_tag.addr_type);
                    bluetooth_release_tag_list();
                    system_status.max_index = ACTION_MENU_COUNT;
                    system_status.screen_id = SCREEN_ACTIONS;
                }
            }
            break;

        case SCREEN_ACTIONS:
            if (bluetooth_is_connected()) {
                if (system_status.selected_index == ACTION_CONTROL_GPIO) {
                    system_status.max_index = GPIO_MENU_COUNT;
                    system_status.screen_id = SCREEN_CONTROL_GPIO;
                }
                else if (system_status.selected_index == ACTION_CONTROL_BLE) {
                    system_status.max_index = BLE_MENU_COUNT;
                    system_status.screen_id = SCREEN_CONTROL_BLE;
                }
                else {
                    /* Back to device list screen */
                    system_status.screen_id = SCREEN_DEVICE_LIST;
                    fill_devices_name();
                }
            }
            else {
                enter_error_screen(ERROR_BLE_CONNECT);
            }
            break;

        case SCREEN_CONTROL_GPIO:
            if (system_status.selected_index == GPIO_CONTROL_OFF) {
                if (main_send_command("OUTPUTS:0")) {
                    system_status.gpio_state = 0;
                }
                return;
            }
            else if (system_status.selected_index == GPIO_CONTROL_ON) {
                if (main_send_command("OUTPUTS:1")) {
                    system_status.gpio_state = 1;
                }
                return;
            }
            else {
                /* Back to actions screen */
                system_status.max_index = ACTION_MENU_COUNT;
                system_status.screen_id = SCREEN_ACTIONS;
            }
            break;

        case SCREEN_CONTROL_BLE:
            if (system_status.selected_index == BLE_CONTROL_OFF) {
                if (main_send_command("BLE_DELAY:-1")) {
                    system_status.ble_delay = -1;
                }
                return;
            }
            else if (system_status.selected_index == BLE_CONTROL_ON) {
                if (main_send_command("BLE_DELAY:0")) {
                    system_status.ble_delay = 0;
                }
                return;
            }
            else if (system_status.selected_index == BLE_CONTROL_DELAY) {
                system_status.max_index = DELAY_MENU_COUNT;
                system_status.screen_id = SCREEN_SET_DELAY;
            }
            else {
                /* Back to actions screen */
                system_status.max_index = ACTION_MENU_COUNT;
                system_status.screen_id = SCREEN_ACTIONS;
            }
            break;

        case SCREEN_SET_DELAY:
            if (system_status.selected_index < DELAY_CONTROL_OK) {
                return;  /* Do nothing */
            }
            else {
                if (system_status.selected_index == DELAY_CONTROL_OK) {
                    LOG_PRINTF("Set BLE delay %d minutes\n", system_status.set_ble_delay);
                    char cmd[16];
                    sprintf(cmd, "BLE_DELAY:%d", system_status.set_ble_delay);
                    if (main_send_command(cmd)) {
                        system_status.ble_delay = system_status.set_ble_delay;
                    }
                }

                /* Back to BLE screen */
                system_status.max_index = BLE_MENU_COUNT;
                system_status.screen_id = SCREEN_CONTROL_BLE;
            }
            break;

        case SCREEN_BLE_ERROR:
            /* Back to ping screen */
            system_status.screen_id = SCREEN_PING;
            break;

        default:
            return;
    }

    system_status.selected_index = 0;
}

static void handle_up(void) {
    if (system_status.screen_id == SCREEN_SET_DELAY) {
        if (system_status.selected_index < DELAY_CONTROL_OK) {
            adjust_delay_time(1);
        }
    }
    else {
        decrease_selection();
    }
}

static void handle_down(void) {
    if (system_status.screen_id == SCREEN_SET_DELAY) {
        if (system_status.selected_index < DELAY_CONTROL_OK) {
            adjust_delay_time(-1);
        }
    }
    else {
        increase_selection();
    }
}

static void handle_left(void) {
    decrease_selection();
}

static void handle_right(void) {
    increase_selection();
}

static void handle_select_holding(void) {
    if (system_status.screen_id == SCREEN_SET_DELAY) {
        system_status.max_index = BLE_MENU_COUNT;
        system_status.screen_id = SCREEN_CONTROL_BLE;
    }
    else if ((system_status.screen_id == SCREEN_CONTROL_GPIO) || (system_status.screen_id == SCREEN_CONTROL_BLE)) {
        system_status.max_index = ACTION_MENU_COUNT;
        system_status.screen_id = SCREEN_ACTIONS;
    }
    else if (system_status.screen_id == SCREEN_ACTIONS) {
        system_status.screen_id = SCREEN_DEVICE_LIST;
        fill_devices_name();
        return;
    }
    else {
        system_status.screen_id = SCREEN_PING;
    }
    system_status.selected_index = 0;
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
