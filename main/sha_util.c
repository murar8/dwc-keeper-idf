#include "sha_util.h"

#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "sha_util";

esp_err_t parse_sha256(const char *sha256, size_t sha256_len, uint8_t *sha256_bytes)
{
    if (sha256_len != 65)
    {
        ESP_LOGE(TAG, "parse_sha256: Invalid sha256 length: %d", sha256_len);
        return ESP_FAIL;
    }
    else if (sscanf(sha256,
                    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
                    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
                    &sha256_bytes[0], &sha256_bytes[1], &sha256_bytes[2], &sha256_bytes[3], &sha256_bytes[4],
                    &sha256_bytes[5], &sha256_bytes[6], &sha256_bytes[7], &sha256_bytes[8], &sha256_bytes[9],
                    &sha256_bytes[10], &sha256_bytes[11], &sha256_bytes[12], &sha256_bytes[13], &sha256_bytes[14],
                    &sha256_bytes[15], &sha256_bytes[16], &sha256_bytes[17], &sha256_bytes[18], &sha256_bytes[19],
                    &sha256_bytes[20], &sha256_bytes[21], &sha256_bytes[22], &sha256_bytes[23], &sha256_bytes[24],
                    &sha256_bytes[25], &sha256_bytes[26], &sha256_bytes[27], &sha256_bytes[28], &sha256_bytes[29],
                    &sha256_bytes[30], &sha256_bytes[31]) != 32)
    {
        ESP_LOGE(TAG, "parse_sha256: Invalid sha256");
        return ESP_FAIL;
    }
    else
    {
        return ESP_OK;
    }
}