#pragma once

#include <esp_idf_lib_helpers.h>
#include <hd44780.h>

#define LCD_FONT HD44780_FONT_5X8
#define LCD_LINES 2

hd44780_t *lcd_init();