#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_netif_types.h>
#include <esp_ota_ops.h>

#include "ota.h"
#include "ota_logger.h"

#define ERR_ALREADY_UPDATED 0xC001C0D1

static const char *TAG = "ota";

static esp_http_client_config_t ota_http_config = {
    .url = NULL,
    .timeout_ms = CONFIG_OTA_TIMEOUT_MS,
};

static esp_https_ota_config_t ota_config = {
    .http_config = &ota_http_config,
};

static bool is_same_sha256(esp_app_desc_t *new_app_info)
{
    esp_app_desc_t running_app;
    return esp_ota_get_partition_description(esp_ota_get_running_partition(), &running_app) == ESP_OK &&
           memcmp(new_app_info->app_elf_sha256, running_app.app_elf_sha256, sizeof(new_app_info->app_elf_sha256)) == 0;
}

static esp_err_t ota_update(esp_https_ota_handle_t ota_handle)
{
    ESP_LOGI(TAG, "ota_update: starting...");

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

    if (is_same_sha256(&app_desc))
    {
        ESP_LOGI(TAG, "ota_update: Already up to date, skipping update.");
        return ERR_ALREADY_UPDATED;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update: validate_image_header failed: %s", esp_err_to_name(err));
        return err;
    }

    while (esp_https_ota_perform(ota_handle) == ESP_ERR_HTTPS_OTA_IN_PROGRESS)
    {
        ESP_LOGD(TAG, "ota_update: esp_https_ota_perform: %s", esp_err_to_name(err));
    }

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
    TaskHandle_t ota_logger_task_handle;
    ESP_ERROR_CHECK(
        esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_logger, &ota_logger_task_handle));

    ota_http_config.url = (char *)pvParameter;
    assert(ota_http_config.url != NULL);
    esp_https_ota_handle_t ota_handle;
    esp_err_t err = ota_update(&ota_handle);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "ota_task: ESP_HTTPS_OTA upgrade successful. Rebooting ...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else if (err != ERR_ALREADY_UPDATED)
    {
        ESP_LOGE(TAG, "ota_task: ota_update failed: %s", esp_err_to_name(err));
    }

    if (ota_handle != NULL)
    {
        esp_https_ota_abort(ota_handle);
    }
    if (xTaskGetCurrentTaskHandle() != NULL)
    {
        vTaskDelete(NULL);
    }
    if (ota_logger_task_handle != NULL)
    {
        vTaskDelete(ota_logger_task_handle);
    }
}

void ota_run(const char *payload_url)
{
    xTaskCreate(ota_task, "ota_task", CONFIG_OTA_TASK_STACK_SIZE, (void *)payload_url, CONFIG_OTA_TASK_PRIORITY, NULL);
}