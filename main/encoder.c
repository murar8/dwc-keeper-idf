#include "encoder.h"
#include "input.h"

#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <driver/timer.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <sdkconfig.h>

static bool on_pcnt_reach_encoder(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;
    input_event_type_t event;
    if (edata->watch_point_value == CONFIG_INPUT_ENCODER_STEP_SIZE)
        event = INPUT_EVENT_TYPE_ROTATE_CW;
    else if (edata->watch_point_value == -CONFIG_INPUT_ENCODER_STEP_SIZE)
        event = INPUT_EVENT_TYPE_ROTATE_CCW;
    xQueueSendFromISR(queue, &event, &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

void encoder_init(QueueHandle_t queue)
{
    pcnt_unit_handle_t unit = NULL;
    pcnt_unit_config_t unit_config = {.high_limit = CONFIG_INPUT_ENCODER_STEP_SIZE,
                                      .low_limit = -CONFIG_INPUT_ENCODER_STEP_SIZE};
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &unit));

    pcnt_glitch_filter_config_t filter_config = {.max_glitch_ns = CONFIG_INPUT_ENCODER_GLITCH_FILTER_MAX_NS};
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(unit, &filter_config));

    pcnt_chan_config_t chan_config = {.edge_gpio_num = CONFIG_INPUT_ENCODER_CLK_GPIO_NUM,
                                      .level_gpio_num = CONFIG_INPUT_ENCODER_DATA_GPIO_NUM};
    pcnt_channel_handle_t channel = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(unit, &chan_config, &channel));

    ESP_ERROR_CHECK(
        pcnt_channel_set_edge_action(channel, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));

    ESP_ERROR_CHECK(
        pcnt_channel_set_level_action(channel, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    pcnt_event_callbacks_t cbs = {.on_reach = on_pcnt_reach_encoder};
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(unit, &cbs, queue));

    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unit, CONFIG_INPUT_ENCODER_STEP_SIZE));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unit, -CONFIG_INPUT_ENCODER_STEP_SIZE));

    ESP_ERROR_CHECK(pcnt_unit_enable(unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
    ESP_ERROR_CHECK(pcnt_unit_start(unit));
}
