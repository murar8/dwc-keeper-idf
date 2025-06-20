#include "button.h"
#include "input.h"

#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <driver/pulse_cnt.h>
#include <driver/timer.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <sdkconfig.h>

ESP_EVENT_DEFINE_BASE(INPUT_EVENT);

static gptimer_handle_t timer = NULL;
static int last_button_level = 1; // 1: released, 0: pressed
static int current_button_level = 1;

static void on_button_edge(void *user_ctx)
{
    uint64_t count;
    ESP_ERROR_CHECK(gptimer_get_raw_count(timer, &count));
    current_button_level = gpio_get_level(CONFIG_INPUT_BUTTON_GPIO_NUM);
    if (current_button_level == last_button_level)
        return;
    if (count > 0)
        ESP_ERROR_CHECK(gptimer_set_raw_count(timer, 0));
    else
        ESP_ERROR_CHECK(gptimer_start(timer));
}

static bool on_timer_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    ESP_ERROR_CHECK(gptimer_stop(timer));
    ESP_ERROR_CHECK(gptimer_set_raw_count(timer, 0));
    BaseType_t high_task_wakeup;
    if (current_button_level == last_button_level)
        return false;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;
    last_button_level = current_button_level;
    input_event_type_t event = current_button_level ? INPUT_EVENT_TYPE_BUTTON_RELEASE : INPUT_EVENT_TYPE_BUTTON_PRESS;
    xQueueSendFromISR(queue, &current_button_level, &high_task_wakeup);
    return high_task_wakeup;
}

void button_init(QueueHandle_t queue)
{
    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, .direction = GPTIMER_COUNT_UP, .resolution_hz = 1000000};
    ESP_ERROR_CHECK(gptimer_new_timer(&config, &timer));
    gptimer_event_callbacks_t cbs = {.on_alarm = on_timer_alarm};
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &cbs, queue));
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = CONFIG_INPUT_BUTTON_DEBOUNCE_MS * 1000, .reload_count = 0, .flags = {.auto_reload_on_alarm = 0}};
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_enable(timer));

    ESP_ERROR_CHECK(gpio_set_direction(CONFIG_INPUT_BUTTON_GPIO_NUM, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_INPUT_BUTTON_GPIO_NUM, on_button_edge, queue));
    ESP_ERROR_CHECK(gpio_set_intr_type(CONFIG_INPUT_BUTTON_GPIO_NUM, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_intr_enable(CONFIG_INPUT_BUTTON_GPIO_NUM));
}
