#include <driver/gpio.h>


void init_gpio_input(gpio_num_t gpio_num) {
    gpio_reset_pin(gpio_num);                        // 重置 GPIO
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);   // 设置为输入模式
    gpio_set_pull_mode(gpio_num, GPIO_PULLUP_ONLY);  // 设置为上拉模式
}


void init_gpio_output(gpio_num_t gpio_num) {
    gpio_reset_pin(gpio_num);                         // 重置 GPIO
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);   // 设置为输出模式

    gpio_set_level(gpio_num,0);
}

void gpio_switch(gpio_num_t gpio_num)
{
    static uint8_t i=0;
	switch(i)
	{
		case 0 : gpio_set_level(gpio_num,0);i++;break;
		case 1 : gpio_set_level(gpio_num,1);i=0;break;
	}
}
