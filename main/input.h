#pragma once

#include <driver/gpio.h>
#include <encoder.h>

#define ENCODER_SW_PIN GPIO_NUM_34
#define ENCODER_DT_PIN GPIO_NUM_39
#define ENCODER_CLK_PIN GPIO_NUM_36

void input_init(void);

bool input_receive_event(rotary_encoder_event_t *event, TickType_t timeout);
