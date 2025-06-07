#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "lcd.h"
#include "ota.h"
#include "storage.h"
#include "wifi.h"

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xTaskCreate(&ota_task, "ota_task", 4096, NULL, 5, NULL);
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    nvs_init();
    wifi_init();
    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL);
    hd44780_t *lcd = lcd_init();

    hd44780_clear(lcd);
    hd44780_gotoxy(lcd, 0, 0);
    hd44780_puts(lcd, "Hello World!");
}