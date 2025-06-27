#pragma once

#include <soc/adc_channel.h>
#include <stdint.h>

#define CONFIG_SENSOR_EC_ADC_CHANNEL ADC1_GPIO35_CHANNEL
#define CONFIG_SENSOR_PH_ADC_CHANNEL ADC1_GPIO34_CHANNEL
#define CONFIG_SENSOR_PH_TEMPCO_ADC_CHANNEL ADC1_GPIO36_CHANNEL

typedef enum
{
    SENSOR_EC_ADC_CHANNEL = CONFIG_SENSOR_EC_ADC_CHANNEL,
    SENSOR_PH_ADC_CHANNEL = CONFIG_SENSOR_PH_ADC_CHANNEL,
    SENSOR_PH_TEMPCO_ADC_CHANNEL = CONFIG_SENSOR_PH_TEMPCO_ADC_CHANNEL,
} sensor_adc_channel_t;

void sensors_init();

int16_t sensor_get_value(sensor_adc_channel_t channel);