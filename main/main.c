#include <driver/adc.h>
#include <driver/gpio.h>
#include <driver/mcpwm.h>
#include <esp_idf_lib_helpers.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/gpio_types.h>
#include <hd44780.h>
#include <soc/adc_channel.h>
#include <stdio.h>

#define LED_PIN GPIO_NUM_2

#define EC_SENSOR_ADC1_CHANNEL ADC1_GPIO35_CHANNEL
#define EC_SENSOR_ADC1_ATTEN ADC_ATTEN_DB_11

#define LEVEL_SENSOR_ADC1_CHANNEL ADC1_GPIO34_CHANNEL
#define LEVEL_SENSOR_ADC1_ATTEN ADC_ATTEN_DB_11

#define PH_SENSOR_ADC1_CHANNEL ADC1_GPIO39_CHANNEL
#define PH_SENSOR_ADC1_ATTEN ADC_ATTEN_DB_11

#define PH_TEMP_SENSOR_ADC1_CHANNEL ADC1_GPIO36_CHANNEL
#define PH_TEMP_SENSOR_ADC1_ATTEN ADC_ATTEN_DB_11

#define PUMP_A_PIN GPIO_NUM_18
#define PUMP_A_MCPWM_UNIT MCPWM_UNIT_0
#define PUMP_A_MCPWM_TIMER MCPWM_TIMER_0
#define PUMP_A_MCPWM_OPR MCPWM_OPR_A

#define PUMP_B_PIN GPIO_NUM_4
#define PUMP_B_MCPWM_UNIT MCPWM_UNIT_0
#define PUMP_B_MCPWM_TIMER MCPWM_TIMER_0
#define PUMP_B_MCPWM_OPR MCPWM_OPR_B

#define PUMP_C_PIN GPIO_NUM_22
#define PUMP_C_MCPWM_UNIT MCPWM_UNIT_0
#define PUMP_C_MCPWM_TIMER MCPWM_TIMER_1
#define PUMP_C_MCPWM_OPR MCPWM_OPR_A

#define PUMP_D_PIN GPIO_NUM_23
#define PUMP_D_MCPWM_UNIT MCPWM_UNIT_0
#define PUMP_D_MCPWM_TIMER MCPWM_TIMER_1
#define PUMP_D_MCPWM_OPR MCPWM_OPR_B

#define LCD_RW_PIN GPIO_NUM_26
#define LCD_RS_PIN GPIO_NUM_27
#define LCD_E_PIN GPIO_NUM_25
#define LCD_D4_PIN GPIO_NUM_33
#define LCD_D5_PIN GPIO_NUM_32
#define LCD_D6_PIN GPIO_NUM_13
#define LCD_D7_PIN GPIO_NUM_14
#define LCD_BL_PIN HD44780_NOT_USED

static hd44780_t lcd = {
    .write_cb = NULL,
    .font = HD44780_FONT_5X8,
    .lines = 2,
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

static mcpwm_config_t pwm_config = {
    .frequency = 1000,
    .cmpr_a = 0,
    .cmpr_b = 0,
    .counter_mode = MCPWM_UP_COUNTER,
    .duty_mode = MCPWM_DUTY_MODE_0,
};

void init_lcd()
{
    // Tie RW pin low -- write only
    ESP_ERROR_CHECK(gpio_reset_pin(LCD_RW_PIN));
    ESP_ERROR_CHECK(gpio_set_direction(LCD_RW_PIN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RW_PIN, 0));

    ESP_ERROR_CHECK(hd44780_init(&lcd));

    static const uint8_t BELL_CHAR[] = {0x04, 0x0e, 0x0e, 0x0e, 0x1f, 0x00, 0x04, 0x00};
    ESP_ERROR_CHECK(hd44780_upload_character(&lcd, 0, BELL_CHAR));
    static const uint8_t HOURGLASS_CHAR[] = {0x1f, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x1f, 0x00};
    ESP_ERROR_CHECK(hd44780_upload_character(&lcd, 1, HOURGLASS_CHAR));
}

void init_pumps()
{
    ESP_ERROR_CHECK(mcpwm_gpio_init(PUMP_A_MCPWM_UNIT, PUMP_A_MCPWM_TIMER, PUMP_A_MCPWM_OPR));
    ESP_ERROR_CHECK(mcpwm_gpio_init(PUMP_B_MCPWM_UNIT, PUMP_B_MCPWM_TIMER, PUMP_B_MCPWM_OPR));
    ESP_ERROR_CHECK(mcpwm_gpio_init(PUMP_C_MCPWM_UNIT, PUMP_C_MCPWM_TIMER, PUMP_C_MCPWM_OPR));
    ESP_ERROR_CHECK(mcpwm_gpio_init(PUMP_D_MCPWM_UNIT, PUMP_D_MCPWM_TIMER, PUMP_D_MCPWM_OPR));
    ESP_ERROR_CHECK(mcpwm_init(PUMP_A_MCPWM_UNIT, PUMP_A_MCPWM_TIMER, &pwm_config));
    ESP_ERROR_CHECK(mcpwm_init(PUMP_B_MCPWM_UNIT, PUMP_B_MCPWM_TIMER, &pwm_config));
    ESP_ERROR_CHECK(mcpwm_init(PUMP_C_MCPWM_UNIT, PUMP_C_MCPWM_TIMER, &pwm_config));
    ESP_ERROR_CHECK(mcpwm_init(PUMP_D_MCPWM_UNIT, PUMP_D_MCPWM_TIMER, &pwm_config));
}

void init_led()
{
    gpio_reset_pin(LED_PIN);
    ESP_ERROR_CHECK(gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT));
}

void init_ec_sensor()
{
    ESP_ERROR_CHECK(adc1_config_channel_atten(EC_SENSOR_ADC1_CHANNEL, EC_SENSOR_ADC1_ATTEN));
}

void init_level_sensor()
{
    ESP_ERROR_CHECK(adc1_config_channel_atten(LEVEL_SENSOR_ADC1_CHANNEL, LEVEL_SENSOR_ADC1_ATTEN));
}

void init_ph_sensor()
{
    ESP_ERROR_CHECK(adc1_config_channel_atten(PH_SENSOR_ADC1_CHANNEL, PH_SENSOR_ADC1_ATTEN));
    ESP_ERROR_CHECK(adc1_config_channel_atten(PH_TEMP_SENSOR_ADC1_CHANNEL, PH_TEMP_SENSOR_ADC1_ATTEN));
}

void init_adc()
{
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));
}

void app_main()
{
    init_lcd();
    init_pumps();
    init_led();
    init_ec_sensor();
    init_level_sensor();
    init_ph_sensor();
    init_adc();

    while (1)
    {
        long ec_sensor_value = 0;
        long level_sensor_value = 0;
        long ph_sensor_value = 0;
        long ph_temp_sensor_value = 0;

        const int samples = 5000;
        for (int i = 0; i < samples; i++)
        {
            ec_sensor_value += adc1_get_raw(EC_SENSOR_ADC1_CHANNEL);
            level_sensor_value += adc1_get_raw(LEVEL_SENSOR_ADC1_CHANNEL);
            ph_sensor_value += adc1_get_raw(PH_SENSOR_ADC1_CHANNEL);
            ph_temp_sensor_value += adc1_get_raw(PH_TEMP_SENSOR_ADC1_CHANNEL);
        }

        ec_sensor_value /= samples;
        level_sensor_value /= samples;
        ph_sensor_value /= samples;
        ph_temp_sensor_value /= samples;

        char ec_sensor_value_str[16];
        char level_sensor_value_str[16];
        char ph_sensor_value_str[16];
        char ph_temp_sensor_value_str[16];

        sprintf(ec_sensor_value_str, "%04ld", ec_sensor_value);
        sprintf(level_sensor_value_str, "%04ld", level_sensor_value);
        sprintf(ph_sensor_value_str, "%04ld", ph_sensor_value);
        sprintf(ph_temp_sensor_value_str, "%04ld", ph_temp_sensor_value);

        hd44780_gotoxy(&lcd, 0, 0);
        hd44780_puts(&lcd, "EC: ");
        hd44780_puts(&lcd, ec_sensor_value_str);
        hd44780_puts(&lcd, " L: ");
        hd44780_puts(&lcd, level_sensor_value_str);
        hd44780_gotoxy(&lcd, 0, 1);
        hd44780_puts(&lcd, "pH: ");
        hd44780_puts(&lcd, ph_sensor_value_str);
        hd44780_puts(&lcd, " T: ");
        hd44780_puts(&lcd, ph_temp_sensor_value_str);
    }
}
