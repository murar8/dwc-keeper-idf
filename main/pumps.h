#pragma once

#include <driver/gpio.h>

#define CONFIG_PUMP_A_GPIO_NUM 18
#define CONFIG_PUMP_B_GPIO_NUM 19
#define CONFIG_PUMP_C_GPIO_NUM 21

typedef enum
{
    PUMP_A_GPIO_NUM = CONFIG_PUMP_A_GPIO_NUM,
    PUMP_B_GPIO_NUM = CONFIG_PUMP_B_GPIO_NUM,
    PUMP_C_GPIO_NUM = CONFIG_PUMP_C_GPIO_NUM,
} pump_gpio_num_t;

void pumps_init(void);

esp_err_t pump_set_level(pump_gpio_num_t pump_gpio_num, uint8_t level);

esp_err_t stop_all_pumps(void);