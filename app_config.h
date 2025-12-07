#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_AIRTAG_COUNT          64
#define GAP_SCAN_DURATION         5

/* Utils */
#define CURRENT_TIME_MS()         millis()
#define ELAPSED_TIME_MS(a)        ((millis() - (a)) & 0xFFFFFFFF)

/* GPIO */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define BUTTON_SELECT_PIN 12
#define BUTTON_UP_PIN 14
#define BUTTON_DOWN_PIN 27
#define BUTTON_LEFT_PIN 26
#define BUTTON_RIGHT_PIN 25
#define VIBE 13
#define RED_LED 2

/* Debug */
#define DEBUG_ENABLED 1
#if DEBUG_ENABLED
#define LOG_BEGIN(a)              Serial.begin(a)
#define LOG_PRINT(a)              Serial.print(a)
#define LOG_PRINTLN(a)            Serial.println(a)
#define LOG_PRINTF(...)           Serial.printf(__VA_ARGS__)
#else
#define LOG_BEGIN(a)
#define LOG_PRINT(a)
#define LOG_PRINTLN(a)
#define LOG_PRINTF(a)
#endif
