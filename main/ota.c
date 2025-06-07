#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_netif_types.h>
#include <esp_ota_ops.h>

#include "ota.h"
#include "ota_logger.h"

#define ERR_ALREADY_UPDATED 0xC001C0D1

#define OTA_URL "http://192.168.1.143:8080/dwc_keeper.bin"
#define OTA_TIMEOUT_MS 10000

#define OTA_TASK_STACK_SIZE_BYTES (1024 * 8)
#define OTA_TASK_PRIORITY 5

static const char *TAG = "ota";

static esp_http_client_config_t ota_http_config = {
    .url = OTA_URL,
    .timeout_ms = OTA_TIMEOUT_MS,
    .keep_alive_enable = true,
    .skip_cert_common_name_check = true,
};

static esp_https_ota_config_t ota_config = {
    .http_config = &ota_http_config,
};

static esp_https_ota_handle_t ota_handle = NULL;
static TaskHandle_t ota_task_handle = NULL;
static TaskHandle_t ota_logger_task_handle = NULL;

static bool is_same_sha256(esp_app_desc_t *new_app_info)
{
    esp_app_desc_t running_app;
    return esp_ota_get_partition_description(esp_ota_get_running_partition(), &running_app) == ESP_OK &&
           memcmp(new_app_info->app_elf_sha256, running_app.app_elf_sha256, sizeof(new_app_info->app_elf_sha256)) == 0;
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL)
        return ESP_ERR_INVALID_ARG;
    else if (is_same_sha256(new_app_info))
        return ERR_ALREADY_UPDATED;
    else
        return ESP_OK;
}

static esp_err_t ota_update(void *pvParameter)
{
    ESP_LOGI(TAG, "ota_update");

    esp_err_t err = esp_https_ota_begin(&ota_config, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update: esp_https_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(ota_handle, &app_desc);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update: esp_https_ota_get_img_desc failed: %s", esp_err_to_name(err));
        return err;
    }

    err = validate_image_header(&app_desc);
    if (err == ERR_ALREADY_UPDATED)
    {
        ESP_LOGI(TAG, "ota_update: Current running version is the same as a new. We will not continue the update.");
        return err;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update: validate_image_header failed: %s", esp_err_to_name(err));
        return err;
    }

    do
    {
        err = esp_https_ota_perform(ota_handle);
        ESP_LOGD(TAG, "ota_update: esp_https_ota_perform: %s", esp_err_to_name(err));
    } while (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

    if (!esp_https_ota_is_complete_data_received(ota_handle))
    {
        ESP_LOGE(TAG, "ota_update: Complete data was not received.");
        return err;
    }

    err = esp_https_ota_finish(ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update: esp_https_ota_finish failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

static void ota_task(void *pvParameter)
{
    esp_err_t err = ota_update(NULL);
    if (err != ESP_OK)
    {
        if (err != ERR_ALREADY_UPDATED)
        {
            ESP_LOGE(TAG, "ota_task: ota_update failed: %s", esp_err_to_name(err));
        }
        if (ota_handle != NULL)
        {
            esp_https_ota_abort(ota_handle);
        }
        if (ota_task_handle != NULL)
        {
            vTaskDelete(ota_task_handle);
        }
        if (ota_logger_task_handle != NULL)
        {
            vTaskDelete(ota_logger_task_handle);
        }
    }
    else
    {
        ESP_LOGI(TAG, "ota_task: ESP_HTTPS_OTA upgrade successful. Rebooting ...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "Connected to WiFi, starting OTA task");
        xTaskCreate(&ota_task, TAG, OTA_TASK_STACK_SIZE_BYTES, NULL, OTA_TASK_PRIORITY, &ota_task_handle);
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler);
    }
}

void ota_init(void)
{
    ESP_LOGI(TAG, "OTA init");
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_logger, &ota_logger_task_handle));
}
