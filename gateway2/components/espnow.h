#include "esp_now.h"

extern uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN];

void wifi_init(void);
esp_err_t espnow_init(void);
