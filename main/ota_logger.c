#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_netif_types.h>
#include <esp_ota_ops.h>

static const char *TAG = "ota_logger";

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

void ota_logger_init(void)
{
    ESP_LOGI(TAG, "ota_logger_init");
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_logger, NULL));
}
