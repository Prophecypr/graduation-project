#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "espnow.h"
#include "esp_gpio.h"
#include "m100p.h"

// 定义16字节的AES密钥
uint8_t s_espnow_key[ESP_NOW_KEY_LEN] = { 
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 
};

// 接收方地址
// static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0x84, 0xfc, 0xe6, 0x09, 0xe4, 0xF8};

static const char *TAG = "espnow";
static void espnow_task(void *arg);

/* WiFi should start before using ESPNOW */
void wifi_init(void)
{
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
}

// ESPNOW接收回调函数
static void example_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len > 0) {
        // ESP_LOGI(TAG, "Received data from: " MACSTR ", len: %d", MAC2STR(recv_info->src_addr), len);

        // 处理数据，假设数据包含温度和湿度
        if (len == 3) {
            client_mode=-1;
			WS2812_black=false;
            latest_temp = data[0] * 10 + data[1];
            latest_hum = data[2];
            MQTT_cloud_data_forwarding(latest_mode + 10 , latest_temp , latest_hum);
            ESP_LOGI(TAG, "Processed temperature data: %i.%i", latest_temp / 10 ,latest_temp % 10);
            ESP_LOGI(TAG, "Processed humidity data: %i%%", latest_hum);
			// ESP_LOGI(TAG, "WS2812_black value: %d", WS2812_black);
        }
		else {
			WS2812_black=true;
            ESP_LOGW(TAG, "Received invalid data length");
        }
    }
	else {
		WS2812_black=true;
        ESP_LOGW(TAG, "Received empty data");
    }
}

esp_err_t espnow_init(void)
{

	/* Initialize ESPNOW and register sending and receiving callback function. */
	ESP_ERROR_CHECK(esp_now_init());
	// 注册接受回调函数
	//ESP_ERROR_CHECK(esp_now_register_send_cb(example_espnow_send_cb));
	ESP_ERROR_CHECK(esp_now_register_recv_cb(example_espnow_recv_cb));

	esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        esp_now_deinit();
        return ESP_FAIL;
    }

    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = 1;
    peer->ifidx = ESP_IF_WIFI_STA;
    peer->encrypt = true;  // 启用加密
    memcpy(peer->peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    memcpy(peer->lmk, s_espnow_key, ESP_NOW_KEY_LEN);  // 设置AES密钥

    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

	xTaskCreate(espnow_task, "espnow_task", 2048, NULL, 10, NULL);

	return ESP_OK;
}
