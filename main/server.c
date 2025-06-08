#include "cJSON.h"
#include "esp_chip_info.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_vfs.h"
#include <fcntl.h>
#include <string.h>

static const char *TAG = "server";

static esp_err_t fallback_handler(httpd_req_t *req)
{

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "message", "Not found");
    char *message = cJSON_Print(root);
    httpd_resp_sendstr(req, message);
    cJSON_free(message);
    cJSON_Delete(root);
    httpd_resp_set_status(req, "404");
    return ESP_OK;
}

static esp_err_t start_rest_server(const char *base_path)
{
    if (!base_path)
    {
        ESP_LOGE(TAG, "start_rest_server: base_path is NULL");
        return ESP_FAIL;
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "start_rest_server: starting server");
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t common_get_uri = {.uri = "/*", .method = HTTP_ANY, .handler = fallback_handler};
    httpd_register_uri_handler(server, &common_get_uri);

    return ESP_OK;
}

void server_init(void)
{
    ESP_ERROR_CHECK(start_rest_server("/"));
}