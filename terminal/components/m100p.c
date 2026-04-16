#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "string.h"
#include "stdio.h"
#include "esp_log.h"
#include "m100p.h"
#include "esp_gpio.h"
#include "serial.h"
#include "mbedtls/md.h"


int flag=0;
char cmd_humidity[512];
char cmd_temperature[512];
char cmd_mode[512];
static const char *TAG = "AT_CMD";

void uart_init() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, M100P_TX_PIN, M100P_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendStringData(const char* logName, const char* str)
{
    // 发送字符串
    int txBytes = uart_write_bytes(UART_NUM_1, str, strlen(str));

    // 可以在这里加上日志
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);

    return txBytes;
}

bool send_command_and_wait(const char *cmd)
{
    uint8_t rx_buffer[BUF_SIZE];
    sendStringData(TAG, cmd);
    ESP_LOGI(TAG, "Sent: %s", cmd);
    
    for (int i = 0; i < 3; i++) {  // 最多重试3次
        int rxBytes = uart_read_bytes(UART_NUM_1, rx_buffer, BUF_SIZE - 1, pdMS_TO_TICKS(300));
        if (rxBytes > 0) {
            rx_buffer[rxBytes] = '\0';
            ESP_LOGI(TAG, "Received: %s", rx_buffer);
            ESP_LOG_BUFFER_HEXDUMP(TAG, rx_buffer, rxBytes, ESP_LOG_INFO);
            if (strstr((char *)rx_buffer, "OK") != NULL) {
                return true;
            }
            else if(strstr((char *)rx_buffer, "10.") != NULL) {
                return true;
            }
            else if(strstr((char *)rx_buffer, "OK\r\rERROR") != NULL) {
                return true;
            }
        }
        if(flag==1) {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            ESP_LOGW(TAG, "Retrying command: %s", cmd);
            sendStringData(TAG, cmd);
        }
    }
    return false;
}

void generate_mqtt_params(char *client_id, char *username, char *password)
{
    const char *device_secret = "bc666bfac2d36a3a8e74d47516676e5e";
    const char *product_key = "hk8yVDm91aS";
    const char *device_name = "esp32c3_4g_terminal";
    const char *timestamp = "1660000000";
    char sign_content[256];

    // 拼接 signContent
    snprintf(sign_content, sizeof(sign_content),
             "clientId%sdeviceName%sproductKey%stimestamp%s",
             device_name, device_name, product_key, timestamp);

    // 使用 HMAC-SHA256 计算签名
    unsigned char hmac_output[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)device_secret, strlen(device_secret));
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)sign_content, strlen(sign_content));
    mbedtls_md_hmac_finish(&ctx, hmac_output);
    mbedtls_md_free(&ctx);

    // 转换 HMAC 输出为十六进制字符串
    for (int i = 0; i < 32; i++) {
        snprintf(password + i * 2, 3, "%02x", hmac_output[i]);
    }

    // 生成 ClientID 和 Username
    snprintf(client_id, 128, "%s|securemode=3,signmethod=hmacsha256,timestamp=%s|", device_name, timestamp);
    snprintf(username, 128, "%s&%s", device_name, product_key);
}

void MQTT_cloud_data_forwarding(int mode,int temperature,int humidity)
{
    snprintf(cmd_temperature, sizeof(cmd_temperature), 
    "AT+MPUB=\"/hk8yVDm91aS/esp32c3_4g_terminal/user/temperature/update\",0,0,\"%i.%i\"\r", temperature/10,temperature%10);
    sendStringData(TAG,cmd_temperature);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    snprintf(cmd_humidity, sizeof(cmd_humidity), 
    "AT+MPUB=\"/hk8yVDm91aS/esp32c3_4g_terminal/user/humidity/update\",0,0,\"%i\"\r", humidity);
    sendStringData(TAG,cmd_humidity);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    snprintf(cmd_mode, sizeof(cmd_mode), 
    "AT+MPUB=\"/hk8yVDm91aS/esp32c3_4g_terminal/user/mode/update\",0,0,\"%i\"\r", mode);
    sendStringData(TAG,cmd_mode);
}

int m100p_init()
{
    char mqtt_params[512];
    const char *mqtt_server = "AT+MIPSTART=\"iot-06z00ek538m2kbw.mqtt.iothub.aliyuncs.com\",1883\r";

    char client_id[128];
    char username[128];
    char password[65]; // HMAC-SHA256 的结果是 32 字节（64 字符的十六进制字符串）

    uart_init();

    vTaskDelay(200 / portTICK_PERIOD_MS);
    if (!send_command_and_wait("AT\r")) return 1;

    if (!send_command_and_wait("ATE0\r")) return 2;

    if (send_command_and_wait("AT+MSUB=\"/hk8yVDm91aS/esp32c3_4g_terminal/user/mode/update\",0\r")) {
        gpio_set_level(LED_GPIO,0);
        return -1;
    }
    flag=1;

    vTaskDelay(200 / portTICK_PERIOD_MS);
    if (!send_command_and_wait("AT+ICCID\r")) return 3;

    vTaskDelay(200 / portTICK_PERIOD_MS);
    if (!send_command_and_wait("AT+CGATT?\r")) return 4;

    if (!send_command_and_wait("AT+CSTT=\"\",\"\",\"\"\r")) return 5;

    if (!send_command_and_wait("AT+CIICR\r")) return 6;

    if (!send_command_and_wait("AT+CIFSR\r")) return 7;

    vTaskDelay(200 / portTICK_PERIOD_MS);

    generate_mqtt_params(client_id, username, password);

    snprintf(mqtt_params, sizeof(mqtt_params),
             "AT+MCONFIG=\"%s\",\"%s\",\"%s\"\r",
             client_id, username, password);

    if (!send_command_and_wait(mqtt_params)) return 8;

    vTaskDelay(200 / portTICK_PERIOD_MS);
    if (!send_command_and_wait(mqtt_server)) return 9;

    if (!send_command_and_wait("AT+MCONNECT=1,120\r")) return 10;
    
    gpio_set_level(LED_GPIO,0);

    return 0;
}
