#pragma once

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <BLEDevice.h>
#include <BLEUtils.h>

#define MAX_AIRTAG_COUNT          160
#define GAP_SCAN_DURATION         15

typedef struct {
    int8_t rssi;
    uint8_t status;
    esp_bd_addr_t bda;
    uint32_t last_seen;
} tag_t;

typedef struct {
    tag_t tags[MAX_AIRTAG_COUNT];
    uint8_t count;
} tag_scan_t;

tag_scan_t *bluetooth_get_tag_list(void);
void bluetooth_release_tag_list(void);
void bluetooth_start_scanning(void);
void bluetooth_add_device(uint8_t *address, int8_t rssi, uint8_t status);
void bluetooth_airtag_connect(esp_bd_addr_t mac, esp_ble_addr_type_t addr_type);
void bluetooth_init(void);
