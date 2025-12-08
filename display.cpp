#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "app_config.h"
#include "display.h"

typedef void (*screen_draw_t)(system_status_t *status);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static void display_draw_title(const char *text) {
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 0);
    display.println(text);
}

static void display_draw_footer(const char *text) {
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 54);
    display.println(text);
}

/******************************************************************************/

static void display_draw_ping_screen(system_status_t *status) {
    display_draw_title("Ping");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 14);
    display.println("Devices");
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 26);
    display.println("Select = Ping");
    if (status->last_ping_ms > 0) {
        display.setCursor(0, 38);
        display.print(status->last_device_count);
        display.print(" found ");
        display.print((CURRENT_TIME_MS() - status->last_ping_ms) / 1000);
        display.print("s ago");
    }
}

static void display_draw_scanning_screen(system_status_t *status) {
    int timeout = (CURRENT_TIME_MS() - status->start_scanning_ms) / 1000;
    timeout = GAP_SCAN_DURATION - timeout;
    if (timeout < 1) {
        timeout = 1;
    }

    display_draw_title("Scanning...");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 14);
    display.print("Timeout ");
    display.print(timeout);
    display.println(timeout > 1 ? " seconds" : " second");
    display.setCursor(0, 26);
    display.print("Found ");
    display.print(status->device_count);
    display.print(status->device_count > 1 ? " devices" : " device");
}

static void display_draw_device_list_screen(system_status_t *status) {
    char title[32];
    sprintf(title, "Devices (%d)", status->device_count);
    display_draw_title(title);
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 12);
    display.println("Pick a device");

    int y = 22;
    for (int i = 0; i < MAX_LINES; i++) {
        if (i == status->selected_index) {
            display.setTextColor(BLACK, WHITE);
            display.setCursor(0, y);
            display.println(status->names[i]);
            display.setTextColor(WHITE, BLACK);
        }
        else {
            display.setCursor(0, y);
            if (strlen(status->names[i]) > 1) {
                display.println(status->names[i]);
            }
            else {
                break;
            }
        }
        y += 8;
    }
}

static void display_draw_actions_screen(system_status_t *status) {
    display_draw_title("Actions");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 14);
    display.print("Device: ");
    display.println(status->selected_tag.name);

    const char *options[3] = {"  GPIO", "  BLE", " [ Back ]"};
    int y = 26;
    for (uint8_t i = 0; i < 3; i++) {
        if (i == status->selected_index) {
            display.setTextColor(BLACK, WHITE);
            display.setCursor(0, y);
            display.println(options[i]);
            display.setTextColor(WHITE, BLACK);
        } else {
            display.setCursor(0, y);
            display.println(options[i]);
        }
        y += 10;
    }
}

screen_draw_t screen_draws[SCREEN_COUNT] = {
    display_draw_ping_screen,
    display_draw_scanning_screen,
    display_draw_device_list_screen,
    display_draw_actions_screen,
};

void display_loop(system_status_t *status) {
    static uint32_t last_update_ms = 0;

    /* Update screen every 200ms */
    if (status->force_update || (ELAPSED_TIME_MS(last_update_ms) > 100)) {
        status->force_update = false;
        last_update_ms = CURRENT_TIME_MS();
        display.clearDisplay();

        if (status->screen_id < SCREEN_COUNT) {
            screen_draws[status->screen_id](status);
        }

        display.display();
    }
}

void display_init(void) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (true) {
            delay(10);
        }
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 12);
    display.println("AirSentry");
    display.setTextSize(1);
    display.setCursor(0, 36);
    display.println("Beacon Configuration");
    display.display();
    delay(1200);
}
