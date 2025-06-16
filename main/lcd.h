#pragma once

#include <hd44780.h>

#define LCD_FONT HD44780_FONT_5X8
#define LCD_LINES 2

#define LCD_RW_PIN GPIO_NUM_26
#define LCD_RS_PIN GPIO_NUM_27
#define LCD_E_PIN GPIO_NUM_25
#define LCD_D4_PIN GPIO_NUM_33
#define LCD_D5_PIN GPIO_NUM_32
#define LCD_D6_PIN GPIO_NUM_13
#define LCD_D7_PIN GPIO_NUM_14
#define LCD_BL_PIN HD44780_NOT_USED

hd44780_t *lcd_init();