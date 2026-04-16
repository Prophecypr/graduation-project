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

// 定义16字节的AES密钥
uint8_t s_espnow_key[ESP_NOW_KEY_LEN] = { 
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 
};

// 接收方地址
// uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t s_broadcast_mac1[ESP_NOW_ETH_ALEN] = {0x84, 0xfc, 0xe6, 0x09, 0xe4, 0x98};
uint8_t s_broadcast_mac2[ESP_NOW_ETH_ALEN] = {0x18, 0x8b, 0x0e, 0xaf, 0x37, 0xe8};

static const char *TAG = "espnow";

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

static void example_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
	if (status != ESP_NOW_SEND_SUCCESS)
	{
		gpio_set_level(LED_GPIO,0);
		ESP_LOGE(TAG, "Send error");
	}
	else
	{
		WS2812_black=false;
		gpio_switch(LED_GPIO);
		ESP_LOGI(TAG, "Send success");
	}
}

esp_err_t espnow_init(void)
{

	/* Initialize ESPNOW and register sending and receiving callback function. */
	ESP_ERROR_CHECK(esp_now_init());
	// 注册发送回调函数
	ESP_ERROR_CHECK(esp_now_register_send_cb(example_espnow_send_cb));
	//ESP_ERROR_CHECK(esp_now_register_recv_cb(example_espnow_recv_cb));

	// 将广播对等方信息添加到对等方列表
	esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
	if (peer == NULL)
	{
		ESP_LOGE(TAG, "Malloc peer information fail");
		esp_now_deinit();
		return ESP_FAIL;
	}
	// 添加第一个对等方
	memset(peer, 0, sizeof(esp_now_peer_info_t));
	peer->channel = 1;
	peer->ifidx = ESP_IF_WIFI_STA;
	peer->encrypt = true;
	memcpy(peer->peer_addr, s_broadcast_mac1, ESP_NOW_ETH_ALEN);
	memcpy(peer->lmk, s_espnow_key, ESP_NOW_KEY_LEN);
	ESP_ERROR_CHECK(esp_now_add_peer(peer));

	// 添加第二个对等方
	memset(peer, 0, sizeof(esp_now_peer_info_t));
	peer->channel = 1;
	peer->ifidx = ESP_IF_WIFI_STA;
	peer->encrypt = true;
	memcpy(peer->peer_addr, s_broadcast_mac2, ESP_NOW_ETH_ALEN);
	memcpy(peer->lmk, s_espnow_key, ESP_NOW_KEY_LEN);
	ESP_ERROR_CHECK(esp_now_add_peer(peer));
	free(peer);

	return ESP_OK;
}
