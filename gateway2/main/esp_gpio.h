#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>

#define M100P_TX_PIN        GPIO_NUM_0
#define M100P_RX_PIN        GPIO_NUM_1

#define OLED_SCL            GPIO_NUM_2
#define OLED_SDA            GPIO_NUM_3

#define SERIAL_TX_PIN       GPIO_NUM_4
#define SERIAL_RX_PIN       GPIO_NUM_5

#define LED_GPIO            GPIO_NUM_6

#define WS2812_GPIO 	    GPIO_NUM_8
#define WS2812_LED_NUM      1		//WS2812数量

#define OLED_I2C            I2C_NUM_0

#define EXCHANGE_UART       UART_NUM_0

extern bool WS2812_black;
extern int client_mode;
extern int latest_mode;
extern int latest_temp;
extern int latest_hum;

// static TaskHandle_t ws2812_task_handle = NULL;  // 任务句柄

void init_gpio_input(gpio_num_t gpio_num);
void init_gpio_output(gpio_num_t gpio_num);
void gpio_switch(gpio_num_t gpio_num);
