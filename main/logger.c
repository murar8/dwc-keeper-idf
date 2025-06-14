#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

#include "logger.h"

#define CONFIG_LOGGER_MAX_CLIENTS 3

#define CONFIG_LOGGER_SEND_TO_CLIENTS_TASK_PRIORITY 10
#define CONFIG_LOGGER_SEND_TO_CLIENTS_TASK_STACK_SIZE 2048

#define CONFIG_LOGGER_QUEUE_SIZE 50

static const char *TAG = "logger";

static QueueHandle_t log_queue_handle;

static httpd_req_t *clients[CONFIG_LOGGER_MAX_CLIENTS];

static vprintf_like_t original_vprintf;

static int logger_vprintf(const char *fmt, va_list args)
{
    int res = vprintf(fmt, args);

    if (res > 0)
    {
        char *buffer = malloc(res + 10); // enough space for SSE header
        if (buffer != NULL)
        {
            strcpy(buffer, "data: ");
            vsnprintf(buffer + 6, res + 1, fmt, args);
            strcat(buffer, "\n\n");
            xQueueSend(log_queue_handle, &buffer, pdMS_TO_TICKS(1000));
        }
    }

    return res;
}

static void send_to_clients(void *pvParam)
{
    ESP_LOGI(TAG, "send_to_clients: task created");
    char *buffer;

    while (1)
    {
        if (xQueueReceive(log_queue_handle, &buffer, portMAX_DELAY) == pdTRUE)
        {
            for (int i = 0; i < CONFIG_LOGGER_MAX_CLIENTS; i++)
            {
                if (clients[i] != NULL)
                {
                    esp_err_t err = httpd_resp_send_chunk(clients[i], buffer, HTTPD_RESP_USE_STRLEN);
                    if (err != ESP_OK)
                    {
                        ESP_LOGW(TAG, "Failed to send to client %d, removing", i);
                        httpd_req_async_handler_complete(clients[i]);
                        clients[i] = NULL;
                        continue;
                    }
                }
            }
            {
                free(buffer);
            }
        }
    }
}

void logger_init()
{
    ESP_LOGI("logger_init", "Initializing logger");
    assert(log_queue_handle == NULL);
    assert(original_vprintf == NULL);
    log_queue_handle = xQueueCreate(CONFIG_LOGGER_QUEUE_SIZE, sizeof(char *));
    original_vprintf = esp_log_set_vprintf(logger_vprintf);
    assert(log_queue_handle != NULL);
    xTaskCreate(send_to_clients, "send_to_clients", CONFIG_LOGGER_SEND_TO_CLIENTS_TASK_STACK_SIZE, NULL,
                CONFIG_LOGGER_SEND_TO_CLIENTS_TASK_PRIORITY, NULL);
    ESP_LOGI("logger_init", "Logger initialized");
}

esp_err_t logger_add_client(httpd_req_t *req)
{
    for (int i = 0; i < CONFIG_LOGGER_MAX_CLIENTS; i++)
    {
        if (clients[i] == NULL)
        {
            esp_err_t ret = httpd_req_async_handler_begin(req, &clients[i]);
            if (ret == ESP_OK)
            {
                return ESP_OK;
            }
            else
            {
                ESP_LOGE(TAG, "logger_add_client: httpd_req_async_handler_begin failed with code: %d[%s]", ret,
                         esp_err_to_name(ret));
                return ret;
            }
        }
    }
    // else
    {
        ESP_LOGE(TAG, "Logger: Max clients reached");
        return ESP_FAIL;
    }
}
