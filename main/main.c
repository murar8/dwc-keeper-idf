#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "lcd.h"
#include "logger.h"
#include "server.h"
#include "storage.h"
#include "wifi.h"

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    nvs_init();
    wifi_init();
    logger_init();
    server_init();
    hd44780_t *lcd = lcd_init();
    hd44780_clear(lcd);
}