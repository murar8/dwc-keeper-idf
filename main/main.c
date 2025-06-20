#include "heap_debug.h"
#include "input.h"
#include "lcd.h"
#include "logger.h"
#include "server.h"
#include "storage.h"
#include "wifi.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    nvs_init();
    server_init();
    wifi_init();
    logger_init();
    QueueHandle_t input_queue = input_init();
    hd44780_t *lcd = lcd_init();

    start_heap_monitor();

    int count = 0;
    while (true)
    {
        input_event_type_t event;
        if (xQueueReceive(input_queue, &event, portMAX_DELAY))
        {
            switch (event)
            {
            case INPUT_EVENT_TYPE_BUTTON_PRESS:
                count = 0;
                break;
            case INPUT_EVENT_TYPE_BUTTON_RELEASE:
                ESP_LOGI(TAG, "Button released");
                break;
            case INPUT_EVENT_TYPE_ROTATE_CW:
                count++;
                break;
            case INPUT_EVENT_TYPE_ROTATE_CCW:
                count--;
                break;
            }
            hd44780_clear(lcd);
            hd44780_gotoxy(lcd, 0, 0);
            hd44780_puts(lcd, "Button: ");
            hd44780_gotoxy(lcd, 0, 1);
            hd44780_puts(lcd, "Count: ");
            char buffer[16];
            sprintf(buffer, "%d", count);
            hd44780_puts(lcd, buffer);
        }
    }
}