#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_gpio.h"
#include "serial.h"

void serial_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(EXCHANGE_UART, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(EXCHANGE_UART, &uart_config);
    uart_set_pin(EXCHANGE_UART, SERIAL_TX_PIN, SERIAL_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const uint8_t* data, size_t len)
{
    const int txBytes = uart_write_bytes(EXCHANGE_UART, data, len);
    // ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}
