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

#include "ota.h"

static const char *TAG = "server";

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
    if (ret == ESP_OK)
        // return ESP_FAIL to close underlying socket
        return ESP_FAIL;
    else
        return ret;
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
    if (httpd_req_get_hdr_value_str(req, "payload_url", payload_url, payload_url_len) != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update_handler: No payload url received");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing payload_url header");
    }
    {
        ESP_LOGI(TAG, "ota_update_handler: payload_url: %s", payload_url);
        ota_run(payload_url);
        return httpd_resp_send(req, "OTA update started", HTTPD_RESP_USE_STRLEN);
    }
}

static httpd_handle_t start_webserver()
{
    httpd_handle_t server = NULL;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.uri_match_fn = httpd_uri_match_wildcard;

    extern const unsigned char cert_start[] asm("_binary_cert_pem_start");
    extern const unsigned char cert_end[] asm("_binary_cert_pem_end");
    conf.servercert = cert_start;
    conf.servercert_len = cert_end - cert_start;

    extern const unsigned char key_start[] asm("_binary_key_pem_start");
    extern const unsigned char key_end[] asm("_binary_key_pem_end");
    conf.prvtkey_pem = key_start;
    conf.prvtkey_len = key_end - key_start;

    extern const unsigned char client_cert_start[] asm("_binary_client_crt_start");
    extern const unsigned char client_cert_end[] asm("_binary_client_crt_end");
    conf.cacert_pem = client_cert_start;
    conf.cacert_len = client_cert_end - client_cert_start;

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret)
    {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }

    ESP_LOGI(TAG, "Registering URI handlers");
    static const httpd_uri_t ota_uri = {.uri = "/ota", .method = HTTP_GET, .handler = ota_update_handler};
    static const httpd_uri_t fallback_uri = {.uri = "/*", .method = HTTP_GET, .handler = fallback_handler};
    httpd_register_uri_handler(server, &ota_uri);
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