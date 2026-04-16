#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "nvs_flash.h"

#include "esp_gpio.h"
#include "led_ws2812.h"
#include "gatts_multi_connect.h"
#include "espnow.h"
#include "serial.h"
#include "m100p.h"
#include "oled.h"
#include "decrypt.h"


#define MODE_COUNT 6        //0:default 1:serial    2:zigbee    3:ble   4:wifi  5:4G
bool WS2812_black = false;
int client_mode = 0;
int mode = 0;
int latest_temp = 0;
int latest_hum = 0;
int latest_mode = -1;
float temp_f=0;             // 温度浮点

ws2812_strip_handle_t ws2812_handle = NULL;

void OLED_task(void *arg)
{
    while (1)
    {
        OLED_Clear();
        
        OLED_ShowString(0, 0, "Mode:", OLED_8X16);
        switch (latest_mode) {
            case 1:
                OLED_ShowString(40, 0, "Serial", OLED_8X16);
                break;
            case 2:
                OLED_ShowString(40, 0, "Zigbee", OLED_8X16);
                break;
            case 3:
                OLED_ShowString(40, 0, "BLE", OLED_8X16);
                break;
            case 4:
                OLED_ShowString(40, 0, "WIFI", OLED_8X16);
                break;
            case 5:
                OLED_ShowString(40, 0, "4G", OLED_8X16);
                break;
            default:
                OLED_ShowString(40, 0, "Unknown", OLED_8X16);
                break;
        }

        if(client_mode==-1) {
        OLED_ShowString(0, 20, "Temp:", OLED_8X16);
        OLED_ShowFloatNum(40, 20, latest_temp / 10 + 0.1 * (latest_temp % 10), 2, 1, OLED_8X16);
        OLED_ShowString(0, 40, "Hum:", OLED_8X16);
        OLED_ShowNum(32, 40, latest_hum, 2, OLED_8X16);
        }
        OLED_Update();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void uart_rx_task(void *arg)
{
    // uint8_t src_port = data[2];
    // uint8_t dest_port = data[3];
    // uint16_t dest_addr = data[4] | (data[5] << 8);
    static const char *UART_RX_TAG = "RX_TASK";
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(EXCHANGE_UART, data, RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);
        if (rxBytes > 0 && data[0] == 0xFE && data[rxBytes - 1] == 0xFF && data[1] == 0x05 && data[rxBytes - 2] != 0xFF){
            client_mode=data[6];
            latest_mode=client_mode;
            WS2812_black=false;
            ESP_LOGI(UART_RX_TAG, "Read %d bytes", rxBytes);
            ESP_LOG_BUFFER_HEXDUMP(UART_RX_TAG, data, rxBytes, ESP_LOG_INFO);
        }
        else if (rxBytes > 0 && data[0] == 0xFE && data[rxBytes - 1] == 0xFF && data[1] == 0x07) {
            client_mode=-1;
            WS2812_black=false;
            latest_temp = data[6] * 10+ data[7];  // 存储温度
            latest_hum = data[8];   // 存储湿度
            MQTT_cloud_data_forwarding(latest_mode + 10 , latest_temp , latest_hum);
            ESP_LOGI(UART_RX_TAG, "Processed temperature data: %i.%i", latest_temp / 10 ,latest_temp % 10);
            ESP_LOGI(UART_RX_TAG, "Processed humidity data: %i%%", latest_hum);
            // ESP_LOGI(UART_RX_TAG, "Src Port: 0x%02X, Dest Port: 0x%02X, Dest Addr: 0x%04X", 
            //          src_port, dest_port, dest_addr);
        }
        else if(rxBytes > 0 && rxBytes==3) {
            client_mode=-1;
            data[rxBytes] = 0;
            WS2812_black=false;
            latest_temp = data[0] * 10 + data[1];  // 存储温度
            latest_hum = data[2];   // 存储湿度
            MQTT_cloud_data_forwarding(latest_mode + 10 , latest_temp , latest_hum);
            ESP_LOGI(UART_RX_TAG, "Processed temperature data: %i.%i", latest_temp / 10 ,latest_temp % 10);
            ESP_LOGI(UART_RX_TAG, "Processed humidity data: %i%%", latest_hum);
        }
    }
    free(data);
}

void espnow_task(void *arg)
{
    static const char *ESPNOW_TAG = "espnow";
	ESP_LOGI(ESPNOW_TAG, "Start receiving data");
	while (1)
	{
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void ws2812_task(void *arg)
{
    int index = 0;
    uint32_t r, g, b;

    // 彩虹七色：红、橙、黄、绿、蓝、靛、紫
    uint32_t rainbow_colors[][3] = {
        {255, 0, 0},       // 红色
        {255, 102, 0},     // 橙色
        {255, 255, 0},     // 黄色
        {0, 255, 0},       // 绿色
        {0, 255, 255},     // 青色
        {0, 0, 255},       // 蓝色
        {51, 0, 102},      // 紫色
        {255, 0, 0},       // 红色 (重新回到红色，形成循环)
    };
    uint32_t mode_colors[][3] = {
        {0, 0, 0},         // 无色
        {255, 0, 0},       // 红色
        {255, 255, 0},     // 黄色
        {0, 0, 255},       // 蓝色
        {0, 255, 0},       // 绿色
        {51, 0, 102},      // 紫色
    };
    // 每个颜色渐变的步长
    float transition_step = 0.01f;  // 控制渐变的精度，越小越慢
    int num_colors = sizeof(rainbow_colors) / sizeof(rainbow_colors[0]);

    // 亮度因子，控制灯的亮度，0.0f 为最暗，1.0f 为原亮度
    float brightness_factor = 0.2f;  // 设为 0.2 会使灯稍微暗一些
    while (1) {
        if(WS2812_black) {
            for (index = 0; index < WS2812_LED_NUM; index++) {
                ws2812_write(ws2812_handle, index, 0, 0, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(15));
        }
        else if((0<=client_mode) && (client_mode<=5)) {
            for (index = 0; index < WS2812_LED_NUM; index++) {
                ws2812_write(ws2812_handle, index, mode_colors[client_mode][0], mode_colors[client_mode][1], mode_colors[client_mode][2]);
            }
            vTaskDelay(pdMS_TO_TICKS(15));
        }
        else {
            // 遍历每对相邻的颜色进行渐变
            for (int colorIndex = 0; colorIndex < num_colors - 1; colorIndex++) {
                uint32_t start_r = rainbow_colors[colorIndex][0], start_g = rainbow_colors[colorIndex][1], start_b = rainbow_colors[colorIndex][2];
                uint32_t end_r = rainbow_colors[colorIndex + 1][0], end_g = rainbow_colors[colorIndex + 1][1], end_b = rainbow_colors[colorIndex + 1][2];

                // 插值过渡
                for (float t = 0; t <= 1; t += transition_step) {
                    if ((0 <= client_mode) && (client_mode <= 5)) {
                        break; // 立即退出渐变，进入模式颜色
                    }
                    // 根据 t（比例）计算插值后的颜色
                    r = (uint32_t)((1 - t) * start_r + t * end_r);
                    g = (uint32_t)((1 - t) * start_g + t * end_g);
                    b = (uint32_t)((1 - t) * start_b + t * end_b);

                    // 应用亮度因子
                    r = (uint32_t)(r * brightness_factor);
                    g = (uint32_t)(g * brightness_factor);
                    b = (uint32_t)(b * brightness_factor);

                    // 更新所有LED的颜色
                    for (index = 0; index < WS2812_LED_NUM; index++) {
                        ws2812_write(ws2812_handle, index, r, g, b);
                    }

                    // 控制渐变速度，延迟越大越慢
                    vTaskDelay(pdMS_TO_TICKS(15));  // 延迟时间控制渐变的平滑度
                }
            }
        }
    }
}

void app_main(void)     //ws2812:5  espnow:10   uart_rx:15
{
    // esp_log_level_set("*", ESP_LOG_NONE);
    esp_err_t ret = nvs_flash_init();   // Initialize NVS.

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    init_gpio_output(LED_GPIO);
    gpio_set_level(LED_GPIO,1);

    OLED_Init(OLED_I2C, OLED_ADD, OLED_SCL, OLED_SDA, OLED_SPEED);
    OLED_Clear();
    OLED_ShowString( 0, 0, "Initiating...", OLED_8X16);
    OLED_Update();

    ws2812_init(WS2812_GPIO,WS2812_LED_NUM,&ws2812_handle);

    ble_init();

	wifi_init();     // Initialize WiFi
	espnow_init();   // Initialize ESPNOW

    serial_init();

    xTaskCreate(ws2812_task, "WS2812 Task", 2048, NULL, 20, NULL);
    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 15 , NULL);

    int init_error = m100p_init();
    if(init_error==0 || init_error==-1) {
        xTaskCreate(OLED_task, "OLED_task", 2048, NULL, 15, NULL);
    }
    else {
        OLED_Clear();
        OLED_ShowString( 5, 18, "Initial Fail!!!", OLED_8X16);
        OLED_ShowNum(112, 48, init_error, 2, OLED_8X16);
        OLED_Update();
    }
}
