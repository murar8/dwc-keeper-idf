#include "input.h"

#include <driver/gpio.h>
#include <encoder.h>
#include <esp_attr.h>
#include <esp_log.h>

#define CONFIG_INPUT_ENCODER_QUEUE_SIZE 16
#define CONFIG_INPUT_ENCODER_ACCELERATION_COEFF 10

static const char *TAG = "input";

static QueueHandle_t encoder_queue;

void input_init(void)
{
    encoder_queue = xQueueCreate(CONFIG_INPUT_ENCODER_QUEUE_SIZE, sizeof(rotary_encoder_event_t));
    assert(encoder_queue != NULL);
    ESP_ERROR_CHECK(rotary_encoder_init(encoder_queue));
    rotary_encoder_t encoder = {.pin_b = ENCODER_CLK_PIN, .pin_a = ENCODER_DT_PIN, .pin_btn = ENCODER_SW_PIN};
    ESP_ERROR_CHECK(rotary_encoder_add(&encoder));
    ESP_LOGI(TAG, "encoder_init: done");
}

bool input_receive_event(rotary_encoder_event_t *event, TickType_t timeout)
{
    return xQueueReceive(encoder_queue, event, timeout);
}
