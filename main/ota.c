#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_netif_types.h>
#include <esp_ota_ops.h>

#include "ota.h"
#include "ota_logger.h"
#include "utils.h"

#define ERR_ALREADY_UPDATED 0xC001C0D1

#define SHA256_CHAR_LENGTH 32

static const char *TAG = "ota";

extern const char client_cert_start[] asm("_binary_client_crt_start");

static esp_http_client_config_t ota_http_config = {
    .url = NULL,
    .timeout_ms = CONFIG_OTA_TIMEOUT_MS,
    .cert_pem = client_cert_start,
};

static esp_https_ota_config_t ota_config = {
    .http_config = &ota_http_config,
    .bulk_flash_erase = true,
};

static esp_err_t get_current_sha256(char sha256[32])
{
    esp_app_desc_t running_app;
    esp_err_t ret = esp_ota_get_partition_description(esp_ota_get_running_partition(), &running_app);
    LOG_AND_RETURN_IF_ERR("get_current_sha256", "esp_ota_get_partition_description", ret);
    memcpy(sha256, running_app.app_elf_sha256, sizeof(running_app.app_elf_sha256));
    return ret;
}

static esp_err_t ota_update(esp_https_ota_handle_t ota_handle)
{
    esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);
    LOG_AND_RETURN_IF_ERR("ota_update", "esp_https_ota_begin", ret);

    esp_app_desc_t app_desc;
    ret = esp_https_ota_get_img_desc(ota_handle, &app_desc);
    LOG_AND_RETURN_IF_ERR("ota_update", "esp_https_ota_get_img_desc", ret);

    bool is_same;
    ret = ota_is_image_up_to_date(app_desc.app_elf_sha256, &is_same);
    LOG_AND_RETURN_IF_ERR("ota_update", "ota_is_image_up_to_date", ret);
    if (is_same)
    {
        return ERR_ALREADY_UPDATED;
    }

    while (esp_https_ota_perform(ota_handle) == ESP_ERR_HTTPS_OTA_IN_PROGRESS)
    {
        ESP_LOGD(TAG, "ota_update: esp_https_ota_perform: %s", esp_err_to_name(ret));
    }

    if (!esp_https_ota_is_complete_data_received(ota_handle))
    {
        ESP_LOGE(TAG, "ota_update: Complete data was not received.");
        return ESP_FAIL;
    }

    ret = esp_https_ota_finish(ota_handle);
    LOG_AND_RETURN_IF_ERR("ota_update", "esp_https_ota_finish", ret);

    return ESP_OK;
}

static void ota_task(void *payload_url)
{
    TaskHandle_t ota_logger_task_handle;
    ESP_ERROR_CHECK(
        esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_logger, &ota_logger_task_handle));

    assert(payload_url != NULL);
    ota_http_config.url = (char *)payload_url;
    ESP_LOGI(TAG, "ota_task: Starting OTA update from %s", ota_http_config.url);
    esp_https_ota_handle_t ota_handle;
    esp_err_t ret = ota_update(&ota_handle);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "ota_task: ESP_HTTPS_OTA upgrade successful. Rebooting ...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else if (ret == ERR_ALREADY_UPDATED)
    {
        ESP_LOGI(TAG, "ota_update: Already up to date, skipping update.");
    }
    else
    {
        ESP_LOGE(TAG, "ota_task: ota_update failed: %s", esp_err_to_name(ret));
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
    if (payload_url != NULL)
    {
        free(payload_url);
    }
}

esp_err_t ota_is_image_up_to_date(uint8_t sha256[32], bool *is_up_to_date)
{
    char current_sha256[SHA256_CHAR_LENGTH];
    esp_err_t ret = get_current_sha256(current_sha256);
    LOG_AND_RETURN_IF_ERR("ota_is_image_up_to_date", "get_current_sha256", ret);
    *is_up_to_date = memcmp(sha256, current_sha256, SHA256_CHAR_LENGTH) == 0;
    return ESP_OK;
}

void ota_run(const char *payload_url)
{
    xTaskCreate(ota_task, "ota_task", CONFIG_OTA_TASK_STACK_SIZE, (void *)payload_url, CONFIG_OTA_TASK_PRIORITY, NULL);
}