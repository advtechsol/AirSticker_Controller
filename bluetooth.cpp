#include <Arduino.h>
#include <esp_err.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatt_defs.h>
#include <esp_gatt_common_api.h>
#include "app_config.h"
#include "bluetooth.h"

#define ATS_TAG_HANDLE_ID 14
#define PROFILE_NUM       1
#define PROFILE_A_APP_ID  0

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
};

static SemaphoreHandle_t ble_semaphore;
static tag_scan_t tag_list;
static bool is_scanning = false;
static bool get_server = false;
static esp_bd_addr_t last_tag_addr = {0};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type          = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval      = 0x50,
    .scan_window        = 0x30,
    .scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE
};

static esp_bt_uuid_t nus_service_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid = {.uuid128 = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E},},
};

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,  /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

static esp_gattc_char_elem_t *char_elem_result = NULL;

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
        case ESP_GATTC_OPEN_EVT:
            LOG_PRINTLN("ESP_GATTC_OPEN_EVT");
            break;
        case ESP_GATTC_CONNECT_EVT:{
            LOG_PRINTLN("ESP_GATTC_CONNECT_EVT");
            gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
            memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
            break;
        }
        case ESP_GATTC_DIS_SRVC_CMPL_EVT:
            if (param->dis_srvc_cmpl.status == ESP_GATT_OK){
                LOG_PRINTLN("ESP_GATTC_DIS_SRVC_CMPL_EVT");
                esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &nus_service_uuid);
            }
            break;
        case ESP_GATTC_CFG_MTU_EVT:
            LOG_PRINTLN("ESP_GATTC_CFG_MTU_EVT");
            break;
        case ESP_GATTC_SEARCH_RES_EVT: {
            LOG_PRINT("UUID len "); LOG_PRINT(p_data->search_res.srvc_id.uuid.len); LOG_PRINT(" UUID "); LOG_PRINTLN(p_data->search_res.srvc_id.uuid.uuid.uuid128[0]);
            if ((p_data->search_res.srvc_id.uuid.len != ESP_UUID_LEN_128) ||
                (memcmp(p_data->search_res.srvc_id.uuid.uuid.uuid128, nus_service_uuid.uuid.uuid128, ESP_UUID_LEN_128))) {
                    break;
            }
            
            LOG_PRINTLN("ESP_GATTC_SEARCH_RES_EVT");
            LOG_PRINT("Server start handle "); LOG_PRINTLN(p_data->search_res.start_handle);
            LOG_PRINT("Server end handle "); LOG_PRINTLN(p_data->search_res.end_handle);
            get_server = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
            break;
        }
        case ESP_GATTC_SEARCH_CMPL_EVT: {
            if (p_data->search_cmpl.status != ESP_GATT_OK){
                LOG_PRINTLN("ESP_GATTC_SEARCH_CMPL_EVT failed");
                esp_ble_gatts_close(gattc_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id);
                break;
            }
            LOG_PRINTLN("ESP_GATTC_SEARCH_CMPL_EVT successfully");
            break;
        }
        case ESP_GATTC_DISCONNECT_EVT:
            LOG_PRINTLN("ESP_GATTC_DISCONNECT_EVT");
            get_server = false;
            gl_profile_tab[PROFILE_A_APP_ID].gattc_if = gattc_if;
            break;
        default:
            break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

static char* ble_get_name(uint8_t *data, uint8_t len, char *out, size_t out_len) {
    uint8_t pos = 0;

    while (pos < len) {
        uint8_t field_len = data[pos];
        if (field_len == 0) break;

        uint8_t type = data[pos + 1];

        if (type == 0x09 || type == 0x08) {
            uint8_t name_len = field_len - 1;
            if (name_len >= out_len) name_len = out_len - 1;
            memcpy(out, &data[pos + 2], name_len);
            out[name_len] = '\0';
            return out;
        }
        pos += field_len + 1;
    }
    return NULL;
}

void bluetooth_add_device(char *name, uint8_t *address, int8_t rssi) {
    int timeout = 0;
    xSemaphoreTake(ble_semaphore, portMAX_DELAY);

    int8_t index = -1;
    int8_t oldest_index = -1;
    uint32_t elapsed_ms;
    uint8_t max_count;
    uint32_t oldest_time = 0;

    if (tag_list.count > 0) {
        max_count = ((tag_list.count < MAX_AIRTAG_COUNT) ? tag_list.count : MAX_AIRTAG_COUNT);
        for (int i = 0; i < max_count; i++) {
            if (
                (tag_list.tags[i].bda[0] == address[0]) &&
                (tag_list.tags[i].bda[1] == address[1]) &&
                (tag_list.tags[i].bda[2] == address[2]) &&
                (tag_list.tags[i].bda[3] == address[3]) &&
                (tag_list.tags[i].bda[4] == address[4]) &&
                (tag_list.tags[i].bda[5] == address[5])
            ) {
                index = i;
                break;
            }
            /* Remove oldest tags */
            else {
                elapsed_ms = (millis() - tag_list.tags[i].last_seen) & 0xFFFFFFFF;
                if (elapsed_ms > oldest_time) {
                    oldest_index = i;
                    oldest_time = elapsed_ms;
                }
            }
        }
    }
    if (index == -1) {
        /* If buff is full or not in scanning, ignore new tag */
        if (tag_list.count < MAX_AIRTAG_COUNT) {
            index = tag_list.count;
            tag_list.count++;
            LOG_PRINT("Add a new device ");
            LOG_PRINTLN(name);
        }
        else if (oldest_index != -1) {
            index = oldest_index;
        }
    }

    xthal_memcpy(tag_list.tags[index].bda, address, 6);
    snprintf(tag_list.tags[index].name, BLE_NAME_MAX_LEN, "%s", name);
    tag_list.tags[index].rssi = rssi;
    tag_list.tags[index].last_seen = CURRENT_TIME_MS();
    xSemaphoreGive(ble_semaphore);
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    static int try_start = 0;

    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
            break;
        }

        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                LOG_PRINTLN("ESP_GAP_BLE_SCAN_START_COMPLETE_EVT failed");
                esp_ble_gattc_close(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id);
                esp_ble_gap_start_scanning(GAP_SCAN_DURATION);
                break;
            }
            LOG_PRINTLN("ESP_GAP_BLE_SCAN_START_COMPLETE_EVT successfully");
            is_scanning = true;
            break;

        /* The scan has aquired results */
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *) param;
            switch (scan_result->scan_rst.search_evt) {
                /* Compare the current packet to what we expect to get */
                case ESP_GAP_SEARCH_INQ_RES_EVT:
                {
                    char dev_name[32];
                    char *name = ble_get_name(scan_result->scan_rst.ble_adv,
                                            scan_result->scan_rst.adv_data_len,
                                            dev_name,
                                            sizeof(dev_name));

                    if ((!name) || (strncmp(name, "ATS", 3) != 0)) {
                        LOG_PRINTLN("Not supported name");
                        return;
                    }

                    bluetooth_add_device(name, scan_result->scan_rst.bda, scan_result->scan_rst.rssi);
                    break;
                }

                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    break;
                default:
                    break;
            }
        } break;

        /* The scan has either stopped successfully or failed */
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                LOG_PRINTLN("ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT failed");
                break;
            }
            LOG_PRINTLN("ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT successfully");
            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            break;

        default:
            break;
    }
}

tag_scan_t *bluetooth_get_tag_list(void) {
    xSemaphoreTake(ble_semaphore, portMAX_DELAY);
    return &tag_list;
}

void bluetooth_release_tag_list(void) {
    xSemaphoreGive(ble_semaphore);
}

void bluetooth_clear_device_list(void) {
    xSemaphoreTake(ble_semaphore, portMAX_DELAY);
    memset(&tag_list, 0, sizeof(tag_list));
    xSemaphoreGive(ble_semaphore);
}

void bluetooth_start_scanning(void) {
    bluetooth_clear_device_list();
    esp_ble_gap_start_scanning(GAP_SCAN_DURATION);
}

void bluetooth_airtag_connect(esp_bd_addr_t mac, esp_ble_addr_type_t addr_type) {
    LOG_PRINTLN("Connecting to device");
    esp_ble_gap_stop_scanning();
    is_scanning = false;
    
    memcpy(last_tag_addr, mac, sizeof(esp_bd_addr_t));
    esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, mac, addr_type, true);
}

void bluetooth_disconnect(void) {
    esp_ble_gap_stop_scanning();
    esp_ble_gatts_close(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id);
    esp_ble_gattc_close(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id);
}

void bluetooth_init(void) {
    esp_err_t ret;
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    BLEDevice::init("");

    /* Register the callback function to the gap module */
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret) {
        LOG_PRINTLN("esp_ble_gap_register_callback failed");
    }

    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret){
        LOG_PRINTLN("esp_ble_gattc_register_callback failed");
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret){
        LOG_PRINTLN("esp_ble_gattc_app_register failed");
    }

    /* Set scanning params */
    ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (ret) {
        LOG_PRINTLN("esp_ble_gap_set_scan_params failed");
    }

    /* Create bluetooth task */
    ble_semaphore = xSemaphoreCreateMutex();
}
