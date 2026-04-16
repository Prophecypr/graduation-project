#include "esp_now.h"

extern uint8_t s_broadcast_mac1[ESP_NOW_ETH_ALEN];
extern uint8_t s_broadcast_mac2[ESP_NOW_ETH_ALEN];
// static TaskHandle_t espnow_task_handle = NULL;

void wifi_init(void);
esp_err_t espnow_init(void);

// #endif
