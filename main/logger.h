#pragma once

#include "esp_http_server.h"

void logger_init(void);

esp_err_t logger_add_client(httpd_req_t *req);

esp_err_t logger_remove_client(httpd_req_t *req);