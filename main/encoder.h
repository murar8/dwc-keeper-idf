#pragma once

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define CONFIG_INPUT_ENCODER_GLITCH_FILTER_MAX_NS 1000

#define CONFIG_INPUT_ENCODER_STEP_SIZE 2

#define CONFIG_INPUT_ENCODER_CLK_GPIO_NUM GPIO_NUM_36
#define CONFIG_INPUT_ENCODER_DATA_GPIO_NUM GPIO_NUM_39

#define CONFIG_INPUT_PCNT_QUEUE_SIZE 10

void encoder_init(QueueHandle_t queue);