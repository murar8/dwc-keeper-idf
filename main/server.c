#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "sdkconfig.h"
#include <esp_event.h>
#include <esp_https_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <sys/time.h>

#include "ota.h"
#include "utils.h"

static const char *TAG = "server";

static esp_err_t parse_sha256(const char *sha256, size_t sha256_len, uint8_t *sha256_bytes)
{
    if (sha256_len != 65)
    {
        ESP_LOGE(TAG, "parse_sha256: Invalid sha256 length: %d", sha256_len);
        return ESP_FAIL;
    }
    if (sscanf(sha256,
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

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == ESP_HTTPS_SERVER_EVENT)
    {
        if (event_id == HTTPS_SERVER_EVENT_ERROR)
        {
            esp_https_server_last_error_t *last_error = (esp_tls_last_error_t *)event_data;
            ESP_LOGE(TAG, "event_handler: last_error = %s, last_tls_err = %d, tls_flag = %d",
                     esp_err_to_name(last_error->last_error), last_error->esp_tls_error_code,
                     last_error->esp_tls_flags);
        }
    }
}

static esp_err_t fallback_handler(httpd_req_t *req)
{
    esp_err_t ret = httpd_resp_send_404(req);
    LOG_AND_RETURN_IF_ERR("fallback_handler", "httpd_resp_send_404", ret);
    return ESP_FAIL; // return ESP_FAIL to close underlying socket
}

// call ota_task with the provided payload url
static esp_err_t ota_update_handler(httpd_req_t *req)
{
    size_t payload_url_len = 0;
    payload_url_len = httpd_req_get_hdr_value_len(req, "payload_url") + 1;
    if (payload_url_len == 0)
    {
        ESP_LOGE(TAG, "ota_update_handler: No payload url received");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing payload_url header");
    }
    char *payload_url = calloc(1, payload_url_len);
    if (!payload_url)
    {
        ESP_LOGE(TAG, "ota_update_handler: Not enough memory for payload url");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = httpd_req_get_hdr_value_str(req, "payload_url", payload_url, payload_url_len);
    LOG_AND_RETURN_IF_ERR("ota_update_handler", "httpd_req_get_hdr_value_str", ret, free(payload_url));
    ota_run(payload_url); // ota_run will free payload_url
    ret = httpd_resp_send(req, "OTA update started", HTTPD_RESP_USE_STRLEN);
    LOG_AND_RETURN_IF_ERR("ota_update_handler", "httpd_resp_send", ret);
    return ret;
}

static esp_err_t check_image_up_to_date_handler(httpd_req_t *req)
{
    size_t sha256_len = 0;
    sha256_len = httpd_req_get_hdr_value_len(req, "sha256") + 1;
    if (sha256_len == 0)
    {
        ESP_LOGE(TAG, "ota_update_handler: No sha256 received");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing sha256 header");
    }
    char *sha256 = calloc(1, sha256_len);
    if (!sha256)
    {
        ESP_LOGE(TAG, "ota_update_handler: Not enough memory for sha256");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = httpd_req_get_hdr_value_str(req, "sha256", sha256, sha256_len);
    LOG_AND_RETURN_IF_ERR("check_image_up_to_date_handler", "httpd_req_get_hdr_value_str", ret, free(sha256));
    uint8_t sha256_bytes[32];
    ret = parse_sha256(sha256, sha256_len, sha256_bytes);
    free(sha256);
    if (ret != ESP_OK)
    {
        esp_err_t ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid sha256 header");
        LOG_AND_RETURN_IF_ERR("check_image_up_to_date_handler", "httpd_resp_send_err", ret);
        return ret;
    }
    bool is_up_to_date;
    ret = ota_is_image_up_to_date(sha256_bytes, &is_up_to_date);
    if (ret != ESP_OK)
    {
        LOG_AND_RETURN_IF_ERR("check_image_up_to_date_handler", "ota_is_image_up_to_date", ret);
        return ret;
    }
    if (is_up_to_date)
    {
        ESP_LOGI(TAG, "check_image_up_to_date_handler: Image is up to date");
        return httpd_resp_send(req, "Image is up to date", HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        ESP_LOGI(TAG, "check_image_up_to_date_handler: Image is not up to date");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Image is not up to date");
    }
}

static esp_err_t logs_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");

    char sse_data[64];
    while (1)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);             // Get the current time
        int64_t time_since_boot = tv.tv_sec; // Time since boot in seconds
        esp_err_t err;
        int len =
            snprintf(sse_data, sizeof(sse_data), "data: Time since boot: %" PRIi64 " seconds\n\n", time_since_boot);
        if ((err = httpd_resp_send_chunk(req, sse_data, len)) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send sse data (returned %02X)", err);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Send data every second
    }

    httpd_resp_send_chunk(req, NULL, 0); // End response
    return ESP_OK;
}

static httpd_handle_t start_webserver()
{
    httpd_handle_t server = NULL;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.uri_match_fn = httpd_uri_match_wildcard;
    conf.httpd.keep_alive_enable = true;
    conf.httpd.keep_alive_idle = 10;

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

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret)
    {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }

    ESP_LOGI(TAG, "Registering URI handlers");
    static const httpd_uri_t ota_uri = {.uri = "/ota", .method = HTTP_POST, .handler = ota_update_handler};
    static const httpd_uri_t check_image_up_to_date_uri = {
        .uri = "/ota/check", .method = HTTP_GET, .handler = check_image_up_to_date_handler};
    static const httpd_uri_t sse = {.uri = "/logs", .method = HTTP_GET, .handler = logs_handler};
    static const httpd_uri_t fallback_uri = {.uri = "/*", .method = HTTP_GET, .handler = fallback_handler};
    httpd_register_uri_handler(server, &ota_uri);
    httpd_register_uri_handler(server, &check_image_up_to_date_uri);
    httpd_register_uri_handler(server, &sse);
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
        if (stop_webserver(*server) == ESP_OK)
        {
            *server = NULL;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to stop https server");
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
}

void server_init(void)
{
    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_SERVER_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
}