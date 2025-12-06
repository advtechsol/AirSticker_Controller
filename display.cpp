#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "app_config.h"
#include "display.h"

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

void display_draw_ping_screen(system_status_t *status) {
    display_draw_title("Ping");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 14);
    display.println("Devices");
    display.setTextSize(1);
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

void display_loop(system_status_t *status, bool force_update) {
    static uint32_t last_update_ms = 0;

    /* Update screen every 200ms */
    if (force_update || (ELAPSED_TIME_MS(last_update_ms) > 100)) {
        last_update_ms = CURRENT_TIME_MS();
        display.clearDisplay();

        switch (status->screen_id) {
            case SCREEN_PING:
                display_draw_ping_screen(status);
                break;

            default:
                break;
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
