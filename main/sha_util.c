#include "sha_util.h"

#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "sha_util";

esp_err_t parse_sha256(const char *sha256, size_t sha256_len, uint8_t *sha256_bytes)
{
    if (sha256_len != SHA256_LENGTH_CHARS)
    {
        ESP_LOGE(TAG, "parse_sha256: Invalid sha256 length: %d", sha256_len);
        return ESP_FAIL;
    }
    for (size_t i = 0;; i++)
    {
        if (i == SHA256_LENGTH_BYTES)
        {
            return ESP_OK;
        }
        else if (sscanf(sha256, "%02hhx", &sha256_bytes[i]) != 1)
        {
            ESP_LOGE(TAG, "parse_sha256: Invalid sha256");
            return ESP_FAIL;
        }
    }
}