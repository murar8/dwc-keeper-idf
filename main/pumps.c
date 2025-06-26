#include "pumps.h"

#include <driver/ledc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/gpio_types.h>
#include <hd44780.h>
#include <soc/adc_channel.h>

#define PUMPS_FREQUENCY 25000
#define PUMPS_TIMER LEDC_TIMER_0
#define PUMPS_MODE LEDC_HIGH_SPEED_MODE
#define PUMPS_DUTY_RES LEDC_TIMER_8_BIT
#define PUMPS_DUTY_MAX (1ul << PUMPS_DUTY_RES) // 256
#define PUMPS_RAMP_UP_TIME_MS 2000

static const char *TAG = "pumps";

static pump_gpio_num_t pump_gpios[] = {PUMP_A_GPIO_NUM, PUMP_B_GPIO_NUM, PUMP_C_GPIO_NUM};

void pumps_init(void)
{
    ESP_LOGI(TAG, "pumps_init: starting");

    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {.speed_mode = PUMPS_MODE,
                                      .duty_resolution = PUMPS_DUTY_RES,
                                      .timer_num = PUMPS_TIMER,
                                      .freq_hz = PUMPS_FREQUENCY,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    for (int i = 0; i < sizeof(pump_gpios) / sizeof(pump_gpios[0]); i++)
    {
        ledc_channel_config_t ledc_channel = {
            .speed_mode = PUMPS_MODE, .timer_sel = PUMPS_TIMER, .channel = i, .gpio_num = pump_gpios[i]};
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }

    ESP_ERROR_CHECK(ledc_fade_func_install(0));

    ESP_LOGI(TAG, "pumps_init: done");
}

esp_err_t pump_set_level(pump_gpio_num_t pump_gpio_num, uint8_t level)
{
    for (int i = 0;; i++)
    {
        if (i == sizeof(pump_gpios) / sizeof(pump_gpios[0]))
        {
            ESP_LOGE(TAG, "pump_set_level: pump_gpio_num is invalid");
            return ESP_ERR_INVALID_ARG;
        }

        if (pump_gpios[i] != pump_gpio_num)
            continue;

        uint32_t current_duty = ledc_get_duty(PUMPS_MODE, i);
        uint32_t duty = (PUMPS_DUTY_MAX * level) / UINT8_MAX;
        if (duty == current_duty)
            return ESP_OK;
        // increment the duty cycle in steps
        else if (duty > current_duty)
        {
            esp_err_t err = ledc_set_fade_with_time(PUMPS_MODE, i, duty, PUMPS_RAMP_UP_TIME_MS);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "pump_set_level: ledc_set_duty_and_update failed");
                return err;
            }
            err = ledc_fade_start(PUMPS_MODE, i, LEDC_FADE_NO_WAIT);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "pump_set_level: ledc_set_duty failed");
                return err;
            }
            else
            {
                return ESP_OK;
            }
        }
        // decrement the duty cycle immediately
        else if (duty < current_duty)
        {
            esp_err_t err = ledc_set_duty_and_update(PUMPS_MODE, i, duty, duty);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "pump_set_level: ledc_set_duty_with_hpoint failed");
                return err;
            }
            else
            {
                return ESP_OK;
            }
        }
    }
}

esp_err_t stop_all_pumps(void)
{
    for (int i = 0;; i++)
    {
        if (i == sizeof(pump_gpios) / sizeof(pump_gpios[0]))
        {
            return ESP_OK;
        }
        esp_err_t err = ledc_set_duty(PUMPS_MODE, i, 0);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "stop_all_pumps: ledc_set_duty failed");
            continue;
        }
        err = ledc_update_duty(PUMPS_MODE, i);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "stop_all_pumps: ledc_update_duty failed");
            continue;
        }
    }
}
