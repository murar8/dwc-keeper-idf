#pragma once

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define CONFIG_INPUT_BUTTON_GPIO_NUM GPIO_NUM_34

#define CONFIG_INPUT_BUTTON_DEBOUNCE_MS 50

void button_init(QueueHandle_t queue);
