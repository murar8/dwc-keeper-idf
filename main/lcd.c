#include "lcd.h"

#include <driver/gpio.h>

static hd44780_t lcd = {
    .write_cb = NULL,
    .font = LCD_FONT,
    .lines = LCD_LINES,
    .pins =
        {
            .rs = LCD_RS_PIN,
            .e = LCD_E_PIN,
            .d4 = LCD_D4_PIN,
            .d5 = LCD_D5_PIN,
            .d6 = LCD_D6_PIN,
            .d7 = LCD_D7_PIN,
            .bl = LCD_BL_PIN,
        },
};

static const uint8_t BELL_CHAR[] = {
    0x04, 0x0e, 0x0e, 0x0e, 0x1f, 0x00, 0x04, 0x00,
};
static const uint8_t HOURGLASS_CHAR[] = {
    0x1f, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x1f, 0x00,
};

hd44780_t *lcd_init()
{
    // Tie RW pin low -- write only
    ESP_ERROR_CHECK(gpio_reset_pin(LCD_RW_PIN));
    ESP_ERROR_CHECK(gpio_set_direction(LCD_RW_PIN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RW_PIN, 0));

    ESP_ERROR_CHECK(hd44780_init(&lcd));

    ESP_ERROR_CHECK(hd44780_upload_character(&lcd, 0, BELL_CHAR));
    ESP_ERROR_CHECK(hd44780_upload_character(&lcd, 1, HOURGLASS_CHAR));

    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 0, 0);

    return &lcd;
}
