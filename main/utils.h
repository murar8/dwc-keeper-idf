#pragma once

#include <esp_err.h>
#include <esp_log.h>

#define LOG_AND_RETURN_IF_ERR(name, method_name, value, ...)                                                           \
    if (value != ESP_OK)                                                                                               \
    {                                                                                                                  \
        ESP_LOGE(TAG, "%s: %s failed with code %d[%s]", name, method_name, value, esp_err_to_name(value));             \
        __VA_ARGS__;                                                                                                   \
        return value;                                                                                                  \
    }
