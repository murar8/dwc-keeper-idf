#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "heap_debug";

void print_heap_info(const char *label)
{
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "=== %s ===", label);
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Minimum free heap: %lu bytes", esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "Largest free block: %u bytes", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG, "Total allocated: %u bytes", heap_info.total_allocated_bytes);
    ESP_LOGI(TAG, "Total free: %u bytes", heap_info.total_free_bytes);
    ESP_LOGI(TAG, "Total blocks: %u", heap_info.total_blocks);
    ESP_LOGI(TAG, "Free blocks: %u", heap_info.free_blocks);
    ESP_LOGI(TAG, "Allocated blocks: %u", heap_info.allocated_blocks);
}

static void heap_monitor_task(void *pvParameters)
{
    while (1)
    {
        print_heap_info("Heap Status");
        vTaskDelay(pdMS_TO_TICKS(10000)); // Print every 10 seconds
    }
}

void start_heap_monitor(void)
{
    xTaskCreate(heap_monitor_task, "heap_monitor", 2048, NULL, 1, NULL);
}