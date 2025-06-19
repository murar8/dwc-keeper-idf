#pragma once

#include <esp_event.h>

void event_logger(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
