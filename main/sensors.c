#include "sensors.h"

#include <esp_adc/adc_continuous.h>
#include <esp_log.h>
#include <soc/adc_channel.h>
#include <soc/soc_caps.h>

#define CONFIG_SENSORS_PH_ADC_CHANNEL 0
#define CONFIG_SENSORS_PH_TEMPCO_ADC_CHANNEL 0
#define CONFIG_SENSORS_EC_ADC_CHANNEL ADC1_GPIO35_CHANNEL

#define CONFIG_SENSORS_PH_ADC_ATTEN ADC_ATTEN_DB_12
#define CONFIG_SENSORS_PH_TEMPCO_ADC_ATTEN ADC_ATTEN_DB_12
#define CONFIG_SENSORS_EC_ADC_ATTEN ADC_ATTEN_DB_12

#define SENSORS_ADC_UNIT ADC_UNIT_1
#define SENSORS_ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define SENSORS_ADC_BIT_WIDTH ADC_BITWIDTH_12
#define SENSORS_ADC_CONV_FRAME_SIZE SOC_ADC_DIGI_DATA_BYTES_PER_CONV * 1024
#define SENSORS_ADC_MAX_STORE_BUF_SIZE SENSORS_ADC_CONV_FRAME_SIZE * 2
#define SENSORS_ADC_SAMPLE_FREQ_HZ SOC_ADC_SAMPLE_FREQ_THRES_LOW
#define SENSORS_ADC_FORMAT ADC_DIGI_OUTPUT_FORMAT_TYPE1

typedef struct
{
    adc_channel_t adc_channel;
    adc_atten_t adc_atten;
} sensor_config_t;

static const char *TAG = "sensors";

static const sensor_config_t sensor_configs[] = {
    {
        .adc_channel = CONFIG_SENSORS_EC_ADC_CHANNEL,
        .adc_atten = CONFIG_SENSORS_EC_ADC_ATTEN,
    },
    // {
    //     .adc_channel = CONFIG_SENSORS_PH_ADC_CHANNEL,
    //     .adc_atten = CONFIG_SENSORS_PH_ADC_ATTEN,
    // },
    // {
    //     .adc_channel = CONFIG_SENSORS_PH_TEMPCO_ADC_CHANNEL,
    //     .adc_atten = CONFIG_SENSORS_PH_TEMPCO_ADC_ATTEN,
    // },
};

static bool on_adc_continuous_data_ready(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata,
                                         void *user_data)
{
    adc_digi_output_data_t *p = (adc_digi_output_data_t *)&edata->conv_frame_buffer[0];
    ESP_DRAM_LOGI(TAG, "sensors_init: channel: %d, data: %d", p->type1.channel, p->type1.data);
    return false;
}

void sensors_init()
{
    ESP_LOGI(TAG, "sensors_init: starting");

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = SENSORS_ADC_MAX_STORE_BUF_SIZE,
        .conv_frame_size = SENSORS_ADC_CONV_FRAME_SIZE,
    };
    adc_continuous_handle_t handle = NULL;
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    static const int config_count = sizeof(sensor_configs) / sizeof(sensor_configs[0]);
    adc_digi_pattern_config_t adc_pattern[config_count];
    for (int i = 0; i < config_count; i++)
    {
        ESP_LOGI(TAG, "sensors_init: configuring sensor %d", i);
        adc_pattern[i].unit = SENSORS_ADC_UNIT;
        adc_pattern[i].bit_width = SENSORS_ADC_BIT_WIDTH;
        adc_pattern[i].atten = sensor_configs[i].adc_atten;
        adc_pattern[i].channel = sensor_configs[i].adc_channel;
    }
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SENSORS_ADC_SAMPLE_FREQ_HZ,
        .conv_mode = SENSORS_ADC_CONV_MODE,
        .format = SENSORS_ADC_FORMAT,
        .pattern_num = config_count,
        .adc_pattern = adc_pattern,
    };
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    adc_continuous_evt_cbs_t evt_cbs = {.on_conv_done = on_adc_continuous_data_ready};
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &evt_cbs, NULL));

    ESP_ERROR_CHECK(adc_continuous_start(handle));

    ESP_LOGI(TAG, "sensors_init: done");
}
