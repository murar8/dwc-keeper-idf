#include "sensors.h"

#include <esp_adc/adc_continuous.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <soc/adc_channel.h>
#include <soc/soc_caps.h>

#define CONFIG_SENSOR_BUF_SIZE 8

#define SENSOR_EC_ADC_ATTEN ADC_ATTEN_DB_12
#define SENSOR_PH_ADC_ATTEN ADC_ATTEN_DB_12
#define SENSOR_PH_TEMPCO_ADC_ATTEN ADC_ATTEN_DB_12

#define SENSOR_ADC_UNIT ADC_UNIT_1
#define SENSOR_ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define SENSOR_ADC_BIT_WIDTH ADC_BITWIDTH_12
#define SENSOR_ADC_SAMPLE_FREQ_HZ SOC_ADC_SAMPLE_FREQ_THRES_LOW
#define SENSOR_ADC_FORMAT ADC_DIGI_OUTPUT_FORMAT_TYPE1

#define SENSOR_ADC_SAMPLES_PER_FRAME 4096
#define SENSOR_ADC_CONV_FRAME_SIZE (SOC_ADC_DIGI_DATA_BYTES_PER_CONV * SENSOR_ADC_SAMPLES_PER_FRAME)
#define SENSOR_ADC_MAX_STORE_BUF_SIZE (SENSOR_ADC_CONV_FRAME_SIZE)

#define SENSOR_ADC_MAX_VALUE ((1 << SENSOR_ADC_BIT_WIDTH) - 1)

typedef struct
{
    sensor_adc_channel_t adc_channel;
    adc_atten_t adc_atten;
} sensor_config_t;

static const sensor_config_t sensor_configs[] = {
    {
        .adc_channel = SENSOR_EC_ADC_CHANNEL,
        .adc_atten = SENSOR_EC_ADC_ATTEN,
    },
    {
        .adc_channel = SENSOR_PH_ADC_CHANNEL,
        .adc_atten = SENSOR_PH_ADC_ATTEN,
    },
    {
        .adc_channel = SENSOR_PH_TEMPCO_ADC_CHANNEL,
        .adc_atten = SENSOR_PH_TEMPCO_ADC_ATTEN,
    },
};

#define SENSOR_COUNT (sizeof(sensor_configs) / sizeof(sensor_configs[0]))

static const char *TAG = "sensors";

static size_t buffer_start_indexes[SENSOR_COUNT] = {[0 ... SENSOR_COUNT - 1] = 0};
static size_t buffer_end_indexes[SENSOR_COUNT] = {[0 ... SENSOR_COUNT - 1] = 0};
static uint16_t buffers[SENSOR_COUNT][CONFIG_SENSOR_BUF_SIZE] = {
    [0 ... SENSOR_COUNT - 1] = {[0 ... CONFIG_SENSOR_BUF_SIZE - 1] = 0}};

static inline size_t buffer_length(size_t sensor_idx)
{
    return (buffer_end_indexes[sensor_idx] - buffer_start_indexes[sensor_idx] + CONFIG_SENSOR_BUF_SIZE) %
           CONFIG_SENSOR_BUF_SIZE;
}

static void buffer_put(size_t sensor_idx, uint16_t item)
{
    buffers[sensor_idx][buffer_end_indexes[sensor_idx]] = item;
    buffer_end_indexes[sensor_idx] = (buffer_end_indexes[sensor_idx] + 1) % CONFIG_SENSOR_BUF_SIZE;
    if (buffer_end_indexes[sensor_idx] == buffer_start_indexes[sensor_idx])
        buffer_start_indexes[sensor_idx] = (buffer_start_indexes[sensor_idx] + 1) % CONFIG_SENSOR_BUF_SIZE;
}

static uint16_t buffer_get(size_t sensor_idx, size_t offset)
{
    size_t length = buffer_length(sensor_idx);
    if (length == 0 || offset >= length)
        return 0;
    size_t idx = (buffer_start_indexes[sensor_idx] + offset) % length;
    return buffers[sensor_idx][idx];
}

static ssize_t sensor_get_idx(sensor_adc_channel_t channel)
{
    for (size_t i = 0;; i++)
    {
        if (i == SENSOR_COUNT)
            return -1;
        else if (sensor_configs[i].adc_channel == channel)
            return i;
    }
}

int16_t sensor_get_value(sensor_adc_channel_t channel)
{
    ssize_t sensor_idx = sensor_get_idx(channel);
    if (sensor_idx == -1)
        return -1;
    uint32_t sum = 0;
    size_t length = buffer_length(sensor_idx);
    for (size_t i = 0; i < length; i++)
    {
        sum += buffer_get(sensor_idx, i);
    }
    return (int16_t)(sum / length);
}

static bool on_adc_continuous_data_ready(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata,
                                         void *user_data)
{
    uint32_t num_bytes = edata->size;
    uint32_t num_samples = num_bytes / sizeof(adc_digi_output_data_t);
    adc_digi_output_data_t *p = (adc_digi_output_data_t *)edata->conv_frame_buffer;

    uint32_t totals[SENSOR_COUNT] = {[0 ... SENSOR_COUNT - 1] = 0};
    uint32_t num_samples_per_sensor[SENSOR_COUNT] = {[0 ... SENSOR_COUNT - 1] = 0};
    for (int data_idx = 0; data_idx < num_samples; data_idx++)
    {
        for (size_t sensor_idx = 0; sensor_idx < SENSOR_COUNT; sensor_idx++)
        {
            if (p[data_idx].type1.channel == sensor_configs[sensor_idx].adc_channel)
            {
                uint16_t raw_value = p[data_idx].type1.data;
                totals[sensor_idx] += raw_value;
                num_samples_per_sensor[sensor_idx]++;
            }
        }
    }

    for (size_t sensor_idx = 0; sensor_idx < SENSOR_COUNT; sensor_idx++)
    {
        if (num_samples_per_sensor[sensor_idx] != 0)
            buffer_put(sensor_idx, totals[sensor_idx] / num_samples_per_sensor[sensor_idx]);
    }

    // Must be in DRAM!!!
    uint16_t value = buffer_get(0, 0);
    ESP_DRAM_LOGI(
        TAG,
        "on_adc_continuous_data_ready: %lu | AVG %d | RAW %d | START %d | END %d | SAMPLES %d | TOTALS %d | VALUE %d",
        pdTICKS_TO_MS(xTaskGetTickCountFromISR()), sensor_get_value(SENSOR_EC_ADC_CHANNEL),
        totals[0] / num_samples_per_sensor[0], buffer_start_indexes[0], buffer_end_indexes[0],
        num_samples_per_sensor[0], totals[0], value);

    return true;
}

void sensors_init()
{
    ESP_LOGI(TAG, "sensors_init: starting");

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = SENSOR_ADC_MAX_STORE_BUF_SIZE,
        .conv_frame_size = SENSOR_ADC_CONV_FRAME_SIZE,
    };
    adc_continuous_handle_t handle = NULL;
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_digi_pattern_config_t adc_pattern[SENSOR_COUNT];
    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        ESP_LOGI(TAG, "sensors_init: configuring sensor %d", i);
        adc_pattern[i].unit = SENSOR_ADC_UNIT;
        adc_pattern[i].bit_width = SENSOR_ADC_BIT_WIDTH;
        adc_pattern[i].atten = sensor_configs[i].adc_atten;
        adc_pattern[i].channel = sensor_configs[i].adc_channel;
    }
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SENSOR_ADC_SAMPLE_FREQ_HZ,
        .conv_mode = SENSOR_ADC_CONV_MODE,
        .format = SENSOR_ADC_FORMAT,
        .pattern_num = SENSOR_COUNT,
        .adc_pattern = adc_pattern,
    };
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    adc_continuous_evt_cbs_t evt_cbs = {.on_conv_done = on_adc_continuous_data_ready};
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &evt_cbs, NULL));

    ESP_ERROR_CHECK(adc_continuous_start(handle));

    ESP_LOGI(TAG, "sensors_init: done");
}
