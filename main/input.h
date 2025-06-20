#pragma once

#include <esp_event.h>
#include <freertos/queue.h>

typedef enum
{
    INPUT_EVENT_TYPE_BUTTON_PRESS,
    INPUT_EVENT_TYPE_BUTTON_RELEASE,
    INPUT_EVENT_TYPE_ROTATE_CW,
    INPUT_EVENT_TYPE_ROTATE_CCW,
} input_event_type_t;

ESP_EVENT_DECLARE_BASE(INPUT_EVENT);

QueueHandle_t input_init(void);