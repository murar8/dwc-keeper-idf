#pragma once

#include "esp_err.h"

void logger_init(void);
esp_err_t logger_add_socket(int socket);
esp_err_t logger_remove_socket(int socket);