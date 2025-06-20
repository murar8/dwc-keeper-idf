#pragma once

#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>

#define SHA256_LENGTH_BYTES 32
#define SHA256_LENGTH_CHARS (SHA256_LENGTH_BYTES * 2 + 1)

esp_err_t parse_sha256(const char *sha256, size_t sha256_len, uint8_t *sha256_bytes);