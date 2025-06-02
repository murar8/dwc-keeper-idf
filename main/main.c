#include <esp_idf_lib_helpers.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hd44780.h>
#include <stdio.h>

#include "hal/gpio_types.h"

hd44780_t init_lcd()
{
#define LCD_RW_PIN GPIO_NUM_26
#define LCD_RS_PIN GPIO_NUM_27
#define LCD_E_PIN GPIO_NUM_25
#define LCD_D4_PIN GPIO_NUM_33
#define LCD_D5_PIN GPIO_NUM_32
#define LCD_D6_PIN GPIO_NUM_13
#define LCD_D7_PIN GPIO_NUM_14
#define LCD_BL_PIN HD44780_NOT_USED

#define LCD_LINES 2

    // Tie RW pin low -- write only
    gpio_reset_pin(LCD_RW_PIN);
    gpio_set_direction(LCD_RW_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_RW_PIN, 0);

    static hd44780_t lcd = {.write_cb = NULL,
                            .font = HD44780_FONT_5X8,
                            .lines = LCD_LINES,
                            .pins = {.rs = LCD_RS_PIN,
                                     .e = LCD_E_PIN,
                                     .d4 = LCD_D4_PIN,
                                     .d5 = LCD_D5_PIN,
                                     .d6 = LCD_D6_PIN,
                                     .d7 = LCD_D7_PIN,
                                     .bl = LCD_BL_PIN}};

    ESP_ERROR_CHECK(hd44780_init(&lcd));

    static const uint8_t BELL_CHAR[] = {
        0x04, 0x0e, 0x0e, 0x0e, 0x1f, 0x00, 0x04, 0x00,
    };
    static const uint8_t HOURGLASS_CHAR[] = {0x1f, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x1f, 0x00};
    hd44780_upload_character(&lcd, 0, BELL_CHAR);
    hd44780_upload_character(&lcd, 1, HOURGLASS_CHAR);

    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, "\x08 Hello world! \x09");
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, "---");

    return lcd;
}

void app_main()
{
    hd44780_t lcd = init_lcd();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
