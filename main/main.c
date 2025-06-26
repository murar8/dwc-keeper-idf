#include "heap_debug.h"
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

    start_heap_monitor();

    pump_gpio_num_t pump_gpio_num = PUMP_A_GPIO_NUM;
    while (true)
    {
        input_event_type_t event;
        if (xQueueReceive(input_queue, &event, pdMS_TO_TICKS(100)))
        {
            switch (event)
            {
            case INPUT_EVENT_TYPE_BUTTON_PRESS: {
                ESP_LOGI(TAG, "Button Down");
                ESP_ERROR_CHECK(stop_all_pumps());
                if (pump_gpio_num == PUMP_A_GPIO_NUM)
                    pump_gpio_num = PUMP_B_GPIO_NUM;
                else if (pump_gpio_num == PUMP_B_GPIO_NUM)
                    pump_gpio_num = PUMP_C_GPIO_NUM;
                else
                    pump_gpio_num = PUMP_A_GPIO_NUM;
                break;
            }
            case INPUT_EVENT_TYPE_BUTTON_RELEASE: {
                ESP_LOGI(TAG, "Button Up");
                break;
            }
            case INPUT_EVENT_TYPE_ROTATE_CW: {
                ESP_LOGI(TAG, "Rotate CW");
                ESP_ERROR_CHECK(pump_set_level(pump_gpio_num, UINT8_MAX));
                break;
            }
            case INPUT_EVENT_TYPE_ROTATE_CCW: {
                ESP_LOGI(TAG, "Rotate CCW");
                ESP_ERROR_CHECK(pump_set_level(pump_gpio_num, 0));
                break;
            }
            }
            hd44780_gotoxy(lcd, 0, 0);
            char buffer[16];
            sprintf(buffer, "Pump %u", pump_gpio_num);
            hd44780_puts(lcd, buffer);
        }
    }
}