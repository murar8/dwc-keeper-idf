#include "heap_debug.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "heap_debug";

void print_heap_info(const char *label)
{
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
    uint32_t total_allocated_bytes = heap_info.total_allocated_bytes;
    uint32_t total_bytes = (heap_info.total_free_bytes + heap_info.total_allocated_bytes);
    uint32_t minimum_free_bytes = heap_info.minimum_free_bytes;
    uint32_t largest_free_bytes = heap_info.largest_free_block;
    ESP_LOGI(TAG, "%s - Heap Usage: %luB/%luB (min %luB), Blocks: %u/%u (largest free %luB)", label,
             total_allocated_bytes, total_bytes, minimum_free_bytes, heap_info.allocated_blocks, heap_info.total_blocks,
             largest_free_bytes);
}

static void heap_monitor_task(void *pvParameters)
{
    for (;;)
    {
        print_heap_info("monitor");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_HEAP_MONITOR_INTERVAL_MS));
    }
}

void start_heap_monitor(void)
{
    if (CONFIG_HEAP_MONITOR_INTERVAL_MS > 0)
    {
        ESP_LOGI(TAG, "Starting heap monitor task with interval %dms", CONFIG_HEAP_MONITOR_INTERVAL_MS);
        xTaskCreate(heap_monitor_task, "heap_monitor", CONFIG_HEAP_MONITOR_TASK_STACK_SIZE, NULL,
                    CONFIG_HEAP_MONITOR_TASK_PRIORITY, NULL);
    }
    else
    {
        ESP_LOGI(TAG, "Heap monitor disabled");
    }
}