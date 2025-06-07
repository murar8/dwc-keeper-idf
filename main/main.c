#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "lcd.h"
#include "ota.h"
#include "storage.h"
#include "wifi.h"

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    nvs_init();
    wifi_init();
    ota_init();
    hd44780_t *lcd = lcd_init();

    hd44780_clear(lcd);
    hd44780_gotoxy(lcd, 0, 0);
    hd44780_puts(lcd, "Hello World!");

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}