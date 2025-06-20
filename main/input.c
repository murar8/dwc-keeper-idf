#include "input.h"
#include "button.h"
#include "encoder.h"

#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <driver/timer.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <sdkconfig.h>

static const char *TAG = "input";

QueueHandle_t input_init(void)
{
    ESP_LOGI(TAG, "input_init: start");
    QueueHandle_t queue = xQueueCreate(CONFIG_INPUT_PCNT_QUEUE_SIZE, sizeof(int));
    ESP_LOGI(TAG, "input_init: install encoder");
    encoder_init(queue);
    ESP_LOGI(TAG, "input_init: install button");
    button_init(queue);
    ESP_LOGI(TAG, "input_init: done");
    return queue;
}
