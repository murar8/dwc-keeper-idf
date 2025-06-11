#pragma once

void ota_run(const char *payload_url);

esp_err_t ota_is_image_up_to_date(uint8_t sha256[32], bool *is_up_to_date);