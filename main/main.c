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

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    gpio_install_isr_service(0);

    nvs_init();
    wifi_init();
    logger_init();
    server_init();
    hd44780_t *lcd = lcd_init();
    hd44780_puts(lcd, "Hello, World!");
    input_init();
    // start_heap_monitor();

    while (true)
    {
        rotary_encoder_event_t event;
        if (input_receive_event(&event, 0))
        {
            ESP_LOGI("main", "event: %ld, diff: %ld", (long)event.type, (long)event.diff);
        }
        else
        {
            ESP_LOGI("main", "no event");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}