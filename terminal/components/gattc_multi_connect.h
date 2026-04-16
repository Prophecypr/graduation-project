#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_gatt_defs.h"


#define PROFILE_NUM 2
#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1
#define INVALID_HANDLE   0

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

extern struct gattc_profile_inst gl_profile_tab[PROFILE_NUM];

extern esp_gatt_if_t gattc_if1;
extern esp_gatt_if_t gattc_if2;

void ble_init(void);
