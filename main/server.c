#include "logger.h"
#include "ota.h"
#include "sha_util.h"

#include <esp_event.h>
#include <esp_https_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

static const char *TAG = "server";

static esp_err_t fallback_handler(httpd_req_t *req)
{
    esp_err_t ret = httpd_resp_send_404(req);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "fallback_handler: httpd_resp_send_404 failed with code: %d[%s]", ret, esp_err_to_name(ret));
    return ESP_FAIL;
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    esp_err_t ret = httpd_resp_send_404(req);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "favicon_handler: httpd_resp_send_404 failed with code: %d[%s]", ret, esp_err_to_name(ret));
    return ESP_OK;
}

static esp_err_t ota_update_handler(httpd_req_t *req)
{
    if (is_ota_running())
    {
        esp_err_t ret = httpd_resp_send_custom_err(req, "503", "OTA update is already running");
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "ota_update_handler: httpd_resp_send_custom_err failed with code: %d[%s]", ret,
                     esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ota_run();
    esp_err_t ret = httpd_resp_send(req, "OTA update started", HTTPD_RESP_USE_STRLEN);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "ota_update_handler: OTA update started");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "ota_update_handler: httpd_resp_send failed with code: %d[%s]", ret, esp_err_to_name(ret));
        return ESP_FAIL;
    }
}

static esp_err_t check_image_up_to_date_handler(httpd_req_t *req)
{
    if (is_ota_running())
    {
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "OTA update is already running");
    }

    size_t sha256_len = 0;
    sha256_len = httpd_req_get_hdr_value_len(req, "sha256") + 1;
    if (sha256_len == 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing sha256 header");
    }

    char *sha256 = calloc(1, sha256_len);
    if (!sha256)
    {
        ESP_LOGE(TAG, "check_image_up_to_date_handler: Not enough memory for sha256");
        return ESP_FAIL;
    }

    esp_err_t ret = httpd_req_get_hdr_value_str(req, "sha256", sha256, sha256_len);
    if (ret == ESP_ERR_NOT_FOUND)
    {
        esp_err_t http_ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing sha256 header");
        if (http_ret != ESP_OK)
            ESP_LOGE(TAG, "check_image_up_to_date_handler: httpd_resp_send_err failed with code: %d[%s]", http_ret,
                     esp_err_to_name(http_ret));
        free(sha256);
        return ESP_FAIL;
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "check_image_up_to_date_handler: httpd_req_get_hdr_value_str failed with code: %d[%s]", ret,
                 esp_err_to_name(ret));
        free(sha256);
        return ESP_FAIL;
    }

    uint8_t sha256_bytes[32];
    ret = parse_sha256(sha256, sha256_len, sha256_bytes);
    free(sha256);
    if (ret != ESP_OK)
    {
        esp_err_t http_ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid sha256 header");
        if (http_ret != ESP_OK)
            ESP_LOGE(TAG, "check_image_up_to_date_handler: httpd_resp_send_err failed with code: %d[%s]", http_ret,
                     esp_err_to_name(http_ret));
        return ESP_FAIL;
    }

    bool is_up_to_date;
    ret = ota_is_image_up_to_date(sha256_bytes, &is_up_to_date);
    if (ret != ESP_OK)
    {
        esp_err_t http_ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid sha256 header");
        if (http_ret != ESP_OK)
            ESP_LOGE(TAG, "check_image_up_to_date_handler: httpd_resp_send_err failed with code: %d[%s]", http_ret,
                     esp_err_to_name(http_ret));
        return ESP_FAIL;
    }

    if (is_up_to_date)
    {
        ESP_LOGI(TAG, "check_image_up_to_date_handler: Image is up to date");
        esp_err_t ret = httpd_resp_send(req, "Image is up to date", HTTPD_RESP_USE_STRLEN);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "check_image_up_to_date_handler: httpd_resp_send failed with code: %d[%s]", ret,
                     esp_err_to_name(ret));
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGI(TAG, "check_image_up_to_date_handler: Image is not up to date");
        esp_err_t http_ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Image is not up to date");
        if (http_ret != ESP_OK)
            ESP_LOGE(TAG, "check_image_up_to_date_handler: httpd_resp_send_err failed with code: %d[%s]", http_ret,
                     esp_err_to_name(http_ret));
        return ESP_FAIL;
    }
}

static esp_err_t logs_handler(httpd_req_t *in_req)
{
    esp_err_t ret = logger_add_client(in_req);
    if (ret == ESP_OK)
    {
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "logs_handler: logger_add_socket failed with code: %d[%s]", ret, esp_err_to_name(ret));
        return ESP_FAIL;
    }
}

static esp_err_t logs_viewer_handler(httpd_req_t *in_req)
{
    extern const unsigned char index_html_start[] asm("_binary_index_html_start");
    extern const unsigned char index_html_end[] asm("_binary_index_html_end");
    esp_err_t ret = httpd_resp_send(in_req, (const char *)index_html_start, index_html_end - index_html_start);
    if (ret == ESP_OK)
    {
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "logs_viewer_handler: httpd_resp_send failed with code: %d[%s]", ret, esp_err_to_name(ret));
        return ESP_FAIL;
    }
}

static httpd_handle_t start_webserver()
{

    ESP_LOGI(TAG, "Starting server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.uri_match_fn = httpd_uri_match_wildcard;
    conf.httpd.max_open_sockets = CONFIG_HTTP_MAX_OPEN_SOCKETS;

    extern const unsigned char cert_start[] asm("_binary_server_pem_start");
    extern const unsigned char cert_end[] asm("_binary_server_pem_end");
    conf.servercert = cert_start;
    conf.servercert_len = cert_end - cert_start;

    extern const unsigned char key_start[] asm("_binary_server_key_start");
    extern const unsigned char key_end[] asm("_binary_server_key_end");
    conf.prvtkey_pem = key_start;
    conf.prvtkey_len = key_end - key_start;

    extern const unsigned char ca_cert_start[] asm("_binary_ca_pem_start");
    extern const unsigned char ca_cert_end[] asm("_binary_ca_pem_end");
    conf.cacert_pem = ca_cert_start;
    conf.cacert_len = ca_cert_end - ca_cert_start;

    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_ssl_start(&server, &conf));

    ESP_LOGI(TAG, "Registering URI handlers");

    static const httpd_uri_t ota_uri = {.uri = "/ota", .method = HTTP_POST, .handler = ota_update_handler};
    static const httpd_uri_t check_image_up_to_date_uri = {
        .uri = "/ota/check", .method = HTTP_GET, .handler = check_image_up_to_date_handler};
    static const httpd_uri_t sse = {.uri = "/logs", .method = HTTP_GET, .handler = logs_handler};
    static const httpd_uri_t logs_viewer_uri = {
        .uri = "/logs/viewer", .method = HTTP_GET, .handler = logs_viewer_handler};
    static const httpd_uri_t favicon_uri = {.uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler};
    static const httpd_uri_t fallback_uri = {.uri = "/*", .method = HTTP_GET, .handler = fallback_handler};

    httpd_register_uri_handler(server, &ota_uri);
    httpd_register_uri_handler(server, &check_image_up_to_date_uri);
    httpd_register_uri_handler(server, &sse);
    httpd_register_uri_handler(server, &logs_viewer_uri);
    httpd_register_uri_handler(server, &favicon_uri);
    httpd_register_uri_handler(server, &fallback_uri);

    return server;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    return httpd_ssl_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        esp_err_t ret = stop_webserver(*server);
        if (ret == ESP_OK)
        {
            *server = NULL;
        }
        else
        {
            ESP_LOGE(TAG, "disconnect_handler: stop_webserver failed with code: %d[%s]", ret, esp_err_to_name(ret));
        }
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        *server = start_webserver();
    }
    else
    {
        ESP_LOGW(TAG, "connect_handler: server already started");
    }
}

void server_init(void)
{
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
}
