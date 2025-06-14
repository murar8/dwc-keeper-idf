#include "esp_http_server.h"

#pragma once

void logger_init();

esp_err_t logger_add_client(httpd_req_t *req);

void logger_remove_client_by_sockfd(int sockfd);
