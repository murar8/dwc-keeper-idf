#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

void ota_run(void);

esp_err_t ota_is_image_up_to_date(uint8_t sha256[32], bool *is_up_to_date);
bool is_ota_running(void);
