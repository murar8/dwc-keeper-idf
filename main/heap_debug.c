#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "heap_debug";

void print_heap_info(const char *label)
{
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
    uint32_t total_allocated_kib = heap_info.total_allocated_bytes / 1024;
    uint32_t total_kib = (heap_info.total_free_bytes + heap_info.total_allocated_bytes) / 1024;
    uint32_t minimum_free_kib = heap_info.minimum_free_bytes / 1024;
    uint32_t largest_free_kib = heap_info.largest_free_block / 1024;
    ESP_LOGI(TAG, "Heap Usage: %luKiB/%luKiB (min %luKiB), Blocks: %u/%u (largest free %luKiB)", total_allocated_kib,
             total_kib, minimum_free_kib, heap_info.allocated_blocks, heap_info.total_blocks, largest_free_kib);
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