#include "ota.h"
#include "ota_logger.h"
#include "sha_util.h"

#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#define ERR_ALREADY_UPDATED 0xC001C0D1

static const char *TAG = "ota";

extern const char ca_cert_start[] asm("_binary_ca_pem_start");

static esp_http_client_config_t ota_http_config = {
    .url = NULL,
    .timeout_ms = CONFIG_OTA_TIMEOUT_MS,
    .cert_pem = ca_cert_start,
};

static esp_https_ota_config_t ota_config = {
    .http_config = &ota_http_config,
    .bulk_flash_erase = true,
};

static bool s_ota_running = false;
static portMUX_TYPE s_ota_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t get_current_sha256(char sha256[SHA256_LENGTH_BYTES])
{
    esp_app_desc_t running_app;
    esp_err_t ret = esp_ota_get_partition_description(esp_ota_get_running_partition(), &running_app);
    if (ret == ESP_OK)
    {
        memcpy(sha256, running_app.app_elf_sha256, sizeof(running_app.app_elf_sha256));
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "get_current_sha256: esp_ota_get_partition_description failed with code: %d[%s]", ret,
                 esp_err_to_name(ret));
        return ret;
    }
}

static esp_err_t ota_update(esp_https_ota_handle_t ota_handle)
{
    esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update: esp_https_ota_begin failed with code: %d[%s]", ret, esp_err_to_name(ret));
        return ret;
    }

    esp_app_desc_t app_desc;
    ret = esp_https_ota_get_img_desc(ota_handle, &app_desc);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update: esp_https_ota_get_img_desc failed with code: %d[%s]", ret, esp_err_to_name(ret));
        return ret;
    }

    bool is_same;
    ret = ota_is_image_up_to_date(app_desc.app_elf_sha256, &is_same);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update: ota_is_image_up_to_date failed with code: %d[%s]", ret, esp_err_to_name(ret));
        return ret;
    }
    else if (is_same)
    {
        return ERR_ALREADY_UPDATED;
    }

    while (esp_https_ota_perform(ota_handle) == ESP_ERR_HTTPS_OTA_IN_PROGRESS)
    {
        ESP_LOGD(TAG, "ota_update: OTA in progress");
    }

    if (!esp_https_ota_is_complete_data_received(ota_handle))
    {
        ESP_LOGE(TAG, "ota_update: Complete data was not received.");
        return ESP_FAIL;
    }

    ret = esp_https_ota_finish(ota_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ota_update: esp_https_ota_finish failed with code: %d[%s]", ret, esp_err_to_name(ret));
        return ret;
    }
    else
    {
        return ESP_OK;
    }
}

static void ota_task(void *pvParameters)
{
    ESP_LOGI(TAG, "ota_task: Starting OTA update from %s", CONFIG_OTA_PAYLOAD_URL);

    TaskHandle_t ota_logger_task_handle;
    ESP_ERROR_CHECK(
        esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_logger, &ota_logger_task_handle));

    ota_http_config.url = CONFIG_OTA_PAYLOAD_URL;
    esp_https_ota_handle_t ota_handle;
    esp_err_t ret = ota_update(&ota_handle);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "ota_task: ESP_HTTPS_OTA upgrade successful. Rebooting ...");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_OTA_RESTART_DELAY_MS));
        esp_restart();
    }
    else if (ret == ERR_ALREADY_UPDATED)
    {
        ESP_LOGI(TAG, "ota_task: Already up to date, skipping update.");
    }
    else
    {
        ESP_LOGE(TAG, "ota_task: OTA failed with code: %d[%s]", ret, esp_err_to_name(ret));
    }

    s_ota_running = false;
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

esp_err_t ota_is_image_up_to_date(uint8_t sha256[SHA256_LENGTH_BYTES], bool *is_up_to_date)
{
    char current_sha256[SHA256_LENGTH_BYTES];
    esp_err_t ret = get_current_sha256(current_sha256);
    if (ret == ESP_OK)
    {
        *is_up_to_date = memcmp(sha256, current_sha256, SHA256_LENGTH_BYTES) == 0;
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "ota_is_image_up_to_date: get_current_sha256 failed with code: %d[%s]", ret,
                 esp_err_to_name(ret));
        return ret;
    }
}

bool is_ota_running(void)
{
    return s_ota_running;
}

void ota_run(void)
{
    portENTER_CRITICAL(&s_ota_mux);
    if (s_ota_running)
    {
        portEXIT_CRITICAL(&s_ota_mux);
        ESP_LOGE(TAG, "ota_run: OTA is already running");
    }
    else
    {
        s_ota_running = true;
        portEXIT_CRITICAL(&s_ota_mux);
        xTaskCreate(ota_task, "ota_task", CONFIG_OTA_TASK_STACK_SIZE, NULL, CONFIG_OTA_TASK_PRIORITY, NULL);
    }
}