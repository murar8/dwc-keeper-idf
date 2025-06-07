#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_netif_types.h>
#include <esp_ota_ops.h>

#include "ota.h"

#define ERR_ALREADY_UPDATED 0xC001C0DE

static const char *TAG = "ota";

static esp_http_client_config_t ota_http_config = {
    .url = "http://192.168.1.143:8080/dwc_keeper.bin",
    .timeout_ms = 10000,
    .keep_alive_enable = true,
    .skip_cert_common_name_check = true,
};
static esp_https_ota_config_t ota_config = {
    .http_config = &ota_http_config,
};

static void event_logger(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base != ESP_HTTPS_OTA_EVENT)
    {
        return;
    }
    switch (event_id)
    {
    case ESP_HTTPS_OTA_START:
        ESP_LOGI(TAG, "OTA started");
        break;
    case ESP_HTTPS_OTA_CONNECTED:
        ESP_LOGI(TAG, "Connected to server");
        break;
    case ESP_HTTPS_OTA_GET_IMG_DESC:
        ESP_LOGI(TAG, "Reading Image Description");
        break;
    case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
        ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
        break;
    case ESP_HTTPS_OTA_DECRYPT_CB:
        ESP_LOGI(TAG, "Callback to decrypt function");
        break;
    case ESP_HTTPS_OTA_WRITE_FLASH:
        ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
        break;
    case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
        ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
        break;
    case ESP_HTTPS_OTA_FINISH:
        ESP_LOGI(TAG, "OTA finish");
        break;
    case ESP_HTTPS_OTA_ABORT:
        ESP_LOGI(TAG, "OTA abort");
        break;
    }
}

static bool is_updated(esp_app_desc_t *new_app_info)
{
    esp_app_desc_t running_app;
    return esp_ota_get_partition_description(esp_ota_get_running_partition(), &running_app) == ESP_OK &&
           memcmp(new_app_info->app_elf_sha256, running_app.app_elf_sha256, sizeof(new_app_info->app_elf_sha256)) == 0;
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL)
        return ESP_ERR_INVALID_ARG;
    else if (is_updated(new_app_info))
        return ERR_ALREADY_UPDATED;
    else
        return ESP_OK;
}

void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "OTA starting...");

    esp_err_t ota_finish_err = ESP_OK;
    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        goto ota_end;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(ota_handle, &app_desc);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK)
    {
        if (err != ERR_ALREADY_UPDATED)
        {
            ESP_LOGE(TAG, "image header verification failed");
        }
        goto ota_end;
    }

    while (1)
    {
        err = esp_https_ota_perform(ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(ota_handle) != true)
    {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
    }
    else
    {
        ota_finish_err = esp_https_ota_finish(ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK))
        {
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        }
        else
        {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED)
            {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            vTaskDelete(NULL);
        }
    }

ota_end:
    if (err == ERR_ALREADY_UPDATED)
    {
        ESP_LOGI(TAG, "Current running version is the same as a new. We will not continue the update.");
    }
    else
    {
        ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
    }
    if (ota_handle != NULL)
    {
        esp_https_ota_abort(ota_handle);
    }
    vTaskDelete(NULL);
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "Connected to WiFi, starting OTA task");
        xTaskCreate(&ota_task, TAG, 1024 * 8, NULL, 5, NULL);
        vTaskDelete(NULL);
    }
}

void ota_init(void)
{
    ESP_LOGI(TAG, "OTA init");

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_logger, NULL));
}
