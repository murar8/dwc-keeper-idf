#pragma once

void ota_run(const char *payload_url);

//
bool ota_is_image_up_to_date(const char sha256[32]);