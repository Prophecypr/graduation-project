#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string.h>
#include "driver/gpio.h"
#include <esp_log.h>

#include "esp32/rom/ets_sys.h"
#include "esp_gatts_api.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "esp_gattc_api.h"

#include "esp_gpio.h"
#include "dht11.h"
#include "led_ws2812.h"
#include "gattc_multi_connect.h"
#include "espnow.h"
#include "serial.h"
#include "oled.h"
#include "m100p.h"
#include "encrypt.h"


#define MODE_COUNT 6        //0:default 1:serial    2:zigbee    3:ble   4:wifi  5:4G
int client_mode = 0;
uint8_t th_data[3]={0};
uint8_t data[]={0xFE, 0x07, 0x90, 0x91, 0xD7, 0x89, 0x00, 0x00, 0x00, 0xFF};       //Destination：D7 89
int temp = 0,hum = 0;		// 温度 湿度变量
float temp_f=0;             // 温度浮点
bool WS2812_black = true;
bool choose = false;
int display_mode;
bool send_to_d789 = true; // 选择目标设备，默认为 D7 89

ws2812_strip_handle_t ws2812_handle = NULL;

typedef struct {
    bool serial;
    bool zigbee;
    bool ble;
    bool espnow;
    bool _4g;
} TaskState;
TaskState task_state = {false, false, false, false, false};

void uart_tx_task(void *arg);

void OLED_task(void *arg)
{
    while (1)
    {
        OLED_Clear();
        if (choose) {
            OLED_ShowString(0, 0, "Choose Mode:", OLED_8X16);
            switch (client_mode) {
            case 1:
                OLED_ShowString(0, 20, "Serial", OLED_8X16);
                break;
            case 2:
                OLED_ShowString(0, 20, "Zigbee", OLED_8X16);
                break;
            case 3:
                OLED_ShowString(0, 20, "BLE", OLED_8X16);
                break;
            case 4:
                OLED_ShowString(0, 20, "WIFI", OLED_8X16);
                break;
            case 5:
                OLED_ShowString(0, 20, "4G", OLED_8X16);
                break;
            default:
                OLED_ShowString(0, 20, "Unknown", OLED_8X16);
                // ESP_LOGE("OLED_TASK", "Invalid client_mode: %d", client_mode);
                break;
            }

            OLED_ShowString(0, 40, "Target:", OLED_8X16);
            if (send_to_d789) {
                OLED_ShowString(60, 40, "D7 89", OLED_8X16);
            } else {
                OLED_ShowString(60, 40, "50 8D", OLED_8X16);
            }

        }
        else {
            OLED_ShowString(0, 0, "Mode:", OLED_8X16);
            switch (display_mode) {
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
                // ESP_LOGE("OLED_TASK", "Invalid client_mode: %d", client_mode);
                break;
            }

            OLED_ShowString(0, 20, "Temp:", OLED_8X16);
            OLED_ShowFloatNum(40, 20, temp_f, 2, 1, OLED_8X16);
            OLED_ShowString(0, 40, "Hum:", OLED_8X16);
            OLED_ShowNum(32, 40, hum, 2, OLED_8X16);
        }

        OLED_Update();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void button_task(void *arg)
{
    static const char *BUTTON_TAG = "button";
    int mode_state = 0,mode_debounce = 0,mode_pressed = 0;
    int set_state = 0,set_debounce = 0,set_pressed = 0;
    int slave_state = 0, slave_debounce = 0, slave_pressed = 0;

    while (1) {
        mode_state = gpio_get_level(MODE_PIN);
        if (mode_state == 0) {
            mode_debounce++;
            if (mode_debounce >= 10) {
                if (!mode_pressed) {
                    mode_pressed = 1;
                    choose=true;
                    WS2812_black=false;
                    gpio_set_level(LED_GPIO,0);
                    memset(&task_state, 0, sizeof(task_state));
					// ESP_LOGI(BUTTON_TAG,"Button Pressed!\n");
                    client_mode = (client_mode + 1) % MODE_COUNT; // 循环切换模式
                    ESP_LOGI(BUTTON_TAG, "Client Mode: %d\n", client_mode);
                    xTaskCreate(uart_tx_task, "uart_tx_task", 2048, NULL, 15, NULL);
                }
            }
        } else {
            mode_debounce = 0;
            mode_pressed = 0;
        }

        set_state = gpio_get_level(SET_PIN);
        if (set_state == 0) {
            set_debounce++;
            if (set_debounce >= 10) {
                if (!set_pressed) {
                    set_pressed = 1;
                    display_mode=client_mode;
                    switch (client_mode) {
                        case 1:
                            ESP_LOGI(BUTTON_TAG, "Serial mode activated!");
                            task_state.serial = true;
                            break;
                        case 2:
                            ESP_LOGI(BUTTON_TAG, "Zigbee mode activated!");
                            task_state.zigbee = true;
                            break;
                        case 3:
                            ESP_LOGI(BUTTON_TAG, "BLE mode activated!");
                            task_state.ble = true;
                            break;
                        case 4:
                            ESP_LOGI(BUTTON_TAG, "ESP-NOW mode activated!");
                            task_state.espnow = true;
                            break;
                        case 5:
                            ESP_LOGI(BUTTON_TAG, "4G mode activated!");
                            task_state._4g = true;
                            break;
                        default:
                            ESP_LOGI(BUTTON_TAG, "Unknown mode!");
                            ESP_LOGI(BUTTON_TAG, "%d",client_mode);
                            break;
                    }
					// ESP_LOGI(BUTTON_TAG,"Button Pressed!\n");
                    WS2812_black=true;
                    choose=false;
                    xTaskCreate(uart_tx_task, "uart_tx_task", 2048, NULL, 15, NULL);
                }
            }
        } else {
            set_debounce = 0;
            set_pressed = 0;
        }

        slave_state = gpio_get_level(SLAVE_PIN);
        if (slave_state == 0) {
            slave_debounce++;
            if (slave_debounce >= 10) {
                if (!slave_pressed) {
                    slave_pressed = 1;
                    choose=true;
                    send_to_d789 = !send_to_d789; // 切换接收端
                    gpio_switch(LED_GPIO);
                    xTaskCreate(uart_tx_task, "uart_tx_task", 2048, NULL, 15, NULL);
                    ESP_LOGI(BUTTON_TAG, "Target switched to: %s", send_to_d789 ? "D7 89" : "50 8D");
                }
            }
        } else {
            slave_debounce = 0;
            slave_pressed = 0;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void uart_tx_task(void *arg)
{
    static const char *UART_TX_TAG = "TX_TASK";
    unsigned char set[]={0xFE, 0x05, 0x90, 0x91, 0xD7, 0x89, 0x00, 0xFF};       //Destination：D7 89

    if (!send_to_d789) {
        set[4] = 0x50; // 修改目标地址为 50 8D
        set[5] = 0x8D;
    }
    if(choose) {
        set[6]=client_mode;
        sendData(UART_TX_TAG, set ,sizeof(set));
    }
    else {
            set[6]=client_mode;
            // ESP_LOGI(UART_TX_TAG, "UART TX Task Started");
            sendData(UART_TX_TAG, set ,sizeof(set));
            // ESP_LOGI(UART_TX_TAG, "UART TX Task Stopping");
            client_mode=-1;
    }
    vTaskDelete(NULL);
}

void IOT_transmission_task(void *arg)
{
    // uint8_t encrypted_data[sizeof(th_data)];  // 创建存储加密后的数据
    static const char *IOT_TAG = "IOT_TASK";
    while(1)
    {
        if (!send_to_d789) {
            data[4] = 0x50; // 修改目标地址为 50 8D
            data[5] = 0x8D;
        }
        else {
            data[4] = 0xD7; // 修改目标地址为 D7 89
            data[5] = 0x89;
        }
        if (task_state.serial) {
            WS2812_black=false;
            vTaskDelay(150 / portTICK_PERIOD_MS);
            gpio_switch(LED_GPIO);
            sendData(IOT_TAG, th_data , sizeof(th_data));
            vTaskDelay(1850 / portTICK_PERIOD_MS);
        }
        else if (task_state.zigbee) {
            WS2812_black=false;
            vTaskDelay(150 / portTICK_PERIOD_MS);
            gpio_switch(LED_GPIO);
            sendData(IOT_TAG, data , sizeof(data));
            vTaskDelay(1850 / portTICK_PERIOD_MS);
        }
        else if (task_state.ble) {
            WS2812_black=false;
            gpio_switch(LED_GPIO);
            if(send_to_d789)
                esp_ble_gattc_write_char(gattc_if1,
                                         gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                         gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                                         sizeof(th_data),
                                         th_data,
                                         ESP_GATT_WRITE_TYPE_RSP,
                                         ESP_GATT_AUTH_REQ_NONE);
            else if(!send_to_d789) 
                esp_ble_gattc_write_char(gattc_if2,
                                         gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                         gl_profile_tab[PROFILE_B_APP_ID].char_handle,
                                         sizeof(th_data),
                                         th_data,
                                         ESP_GATT_WRITE_TYPE_RSP,
                                         ESP_GATT_AUTH_REQ_NONE);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        else if (task_state.espnow) {
            // ESP_LOGI(IOT_TAG, "Trying to send ESPNOW data...");
            if(send_to_d789)
                esp_now_send(s_broadcast_mac1, (uint8_t *)th_data, sizeof(th_data));
            else if (!send_to_d789)
                esp_now_send(s_broadcast_mac2, (uint8_t *)th_data, sizeof(th_data));
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        else if (task_state._4g) {
            WS2812_black=false;
            gpio_switch(LED_GPIO);
            MQTT_cloud_data_forwarding(5,temp,hum);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void dht11_task(void *arg)
{
	while(1) {	
		DHT11_StartGet(&temp, &hum);
        th_data[0]=temp/10;
        th_data[1]=temp%10;
        th_data[2]=hum;
        data[6]=temp/10;
        data[7]=temp%10;
        data[8]=hum;
        temp_f=temp/10+0.1*(temp%10);
		// printf("temp->%i.%i C     hum->%i%%     temp_f->%.1f\n", temp / 10, temp % 10, hum, temp_f);
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

void app_main(void)     //dht11:5   ws2812:10   uart_tx:15  IOT_transmission_task:20    button:24
{
    // esp_log_level_set("*", ESP_LOG_NONE);
	esp_err_t ret = nvs_flash_init();	// Initialize NVS

	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

    init_gpio_output(LED_GPIO);
    init_gpio_input(MODE_PIN);
    init_gpio_input(SET_PIN);
    init_gpio_input(SLAVE_PIN);

    gpio_set_level(LED_GPIO,1);

    OLED_Init(OLED_I2C, OLED_ADD, OLED_SCL, OLED_SDA, OLED_SPEED);
    OLED_Clear();
    OLED_ShowString( 0, 0, "Initiating...", OLED_8X16);
    OLED_Update();

	DHT11_Init(DHT11_GPIO);

    ws2812_init(WS2812_GPIO,WS2812_LED_NUM,&ws2812_handle);

    ble_init();

	wifi_init();	// Initialize WiFi
	espnow_init();	// Initialize ESPNOW

    serial_init();

	xTaskCreate(dht11_task, "dht11_task", 2048, NULL, 5, NULL);
    xTaskCreate(ws2812_task, "ws2812_task", 2048, NULL, 10, NULL);
    xTaskCreate(button_task, "button_task", 2048, NULL, 24, NULL);
    xTaskCreate(IOT_transmission_task, "IOT_transmission_task", 2048, NULL, 20 , NULL);

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

    vTaskDelay(pdMS_TO_TICKS(15000));
    // Monitor stack usage for a specific task
    UBaseType_t dht11_stack = uxTaskGetStackHighWaterMark(NULL);
    UBaseType_t ws2812_stack = uxTaskGetStackHighWaterMark(NULL);
    UBaseType_t button_stack = uxTaskGetStackHighWaterMark(NULL);
    UBaseType_t iot_transmission_stack = uxTaskGetStackHighWaterMark(NULL);
    
    printf("DHT11 Task Stack Remaining: %u\n", dht11_stack);
    printf("WS2812 Task Stack Remaining: %u\n", ws2812_stack);
    printf("Button Task Stack Remaining: %u\n", button_stack);
    printf("IOT Transmission Task Stack Remaining: %u\n", iot_transmission_stack);
}
