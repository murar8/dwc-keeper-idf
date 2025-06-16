#include "storage.h"

#include <esp_log.h>
#include <nvs_flash.h>

void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    // 1.OTA app partition table has a smaller NVS partition size than the
    // non-OTA partition table. This size mismatch may cause NVS initialization
    // to fail.  2.NVS partition contains data in new format and cannot be
    // recognized by this version of code.  If this happens, we erase NVS
    // partition and initialize NVS again.
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}
