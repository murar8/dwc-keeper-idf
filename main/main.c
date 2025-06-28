#include "input.h"
#include "lcd.h"
#include "logger.h"
#include "pumps.h"
#include "sensors.h"
#include "server.h"
#include "storage.h"
#include "wifi.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define CONFIG_FLOAT_A_GPIO_NUM 22
#define CONFIG_FLOAT_B_GPIO_NUM 23

static const char *TAG = "main";

static void floats_init(void)
{
    ESP_ERROR_CHECK(gpio_set_direction(CONFIG_FLOAT_A_GPIO_NUM, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_direction(CONFIG_FLOAT_B_GPIO_NUM, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(CONFIG_FLOAT_A_GPIO_NUM, GPIO_PULLUP_ONLY));
    ESP_ERROR_CHECK(gpio_set_pull_mode(CONFIG_FLOAT_B_GPIO_NUM, GPIO_PULLUP_ONLY));
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    pumps_init();
    floats_init();
    sensors_init();

    nvs_init();
    server_init();
    wifi_init();
    logger_init();
    QueueHandle_t input_queue = input_init();
    hd44780_t *lcd = lcd_init();

    while (true)
    {
        hd44780_gotoxy(lcd, 0, 0);
        char buffer[16];
        sprintf(buffer, "EC: %d", sensor_get_value(SENSOR_EC_ADC_CHANNEL));
        hd44780_puts(lcd, buffer);
        vTaskDelay(pdMS_TO_TICKS(100));

        input_event_type_t event;
        if (xQueueReceive(input_queue, &event, pdMS_TO_TICKS(100)))
        {
            ESP_LOGI(TAG, "event: %d", event);
        }
    }
}