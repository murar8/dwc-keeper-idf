#include "logger.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "lwip/sys.h"
#include "sdkconfig.h"
#include <lwip/sockets.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "logger";

static RingbufHandle_t log_buffer;

static httpd_req_t *clients[CONFIG_LOG_MAX_SOCKETS] = {NULL};
static SemaphoreHandle_t client_semaphores[CONFIG_LOG_MAX_SOCKETS];

static int logger_vprintf(const char *fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vprintf(fmt, args_copy);
    va_end(args_copy);

    if (len < 0)
        return len;

    char *buffer = malloc(len + 1);

    if (!buffer)
        return len;

    vsnprintf(buffer, len + 1, fmt, args);

    xRingbufferSend(log_buffer, buffer, len + 1, 0 /* no wait */);

    free(buffer);

    return len;
}

esp_err_t logger_add_client(httpd_req_t *req)
{
    for (int i = 0;; i++)
    {
        if (i == CONFIG_LOG_MAX_SOCKETS)
        {
            ESP_LOGE(TAG, "logger_add_client: No free client index found");
            return ESP_ERR_NO_MEM;
        }

        while (!xSemaphoreTake(client_semaphores[i], portMAX_DELAY))
            ESP_LOGD(TAG, "logger_add_client: waiting for semaphore %d", i);

        if (clients[i] != NULL)
        {
            xSemaphoreGive(client_semaphores[i]);
            continue;
        }

        clients[i] = req;
        xSemaphoreGive(client_semaphores[i]);
        ESP_LOGI(TAG, "logger_add_client: client %d added", i);
        return ESP_OK;
    }
}

esp_err_t logger_remove_client(httpd_req_t *req)
{
    for (int i = 0;; i++)
    {
        if (i == CONFIG_LOG_MAX_SOCKETS)
        {
            return ESP_ERR_NOT_FOUND;
        }

        while (!xSemaphoreTake(client_semaphores[i], portMAX_DELAY))
            ESP_LOGD(TAG, "logger_remove_client: waiting for semaphore %d", i);

        if (clients[i] != req)
        {
            xSemaphoreGive(client_semaphores[i]);
            continue;
        }

        httpd_req_t *req = clients[i];
        clients[i] = NULL;
        xSemaphoreGive(client_semaphores[i]);
        ESP_LOGI(TAG, "logger_remove_client: client %d removed", i);
        esp_err_t err = httpd_req_async_handler_complete(req);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "logger_remove_client: httpd_req_async_handler_complete failed with code: %d[%s]", err,
                     esp_err_to_name(err));
        return ESP_OK;
    }
}

static void send_logs_to_clients_task(void *arg)
{
    while (1)
    {
        size_t size;
        char *buffer;
        do
        {
            buffer = xRingbufferReceive(log_buffer, &size, portMAX_DELAY);
        } while (buffer == NULL);

        for (int i = 0; i < CONFIG_LOG_MAX_SOCKETS; i++)
        {
            while (!xSemaphoreTake(client_semaphores[i], portMAX_DELAY))
                ESP_LOGD(TAG, "send_logs_to_clients_task: waiting for semaphore %d", i);

            if (clients[i] == NULL)
            {
                xSemaphoreGive(client_semaphores[i]);
                continue;
            }

            // Format log data as SSE event
            char *sse_buffer = malloc(size + 20); // Extra space for "data: " prefix and "\n\n" suffix
            if (!sse_buffer)
            {
                ESP_LOGE(TAG, "send_logs_to_clients_task: Failed to allocate SSE buffer");
                xSemaphoreGive(client_semaphores[i]);
                continue;
            }

            // Format as SSE: "data: <log message>\n\n"
            int sse_len = snprintf(sse_buffer, size + 20, "data: %.*s\n\n", (int)(size - 1), buffer);

            esp_err_t err = httpd_resp_send_chunk(clients[i], sse_buffer, sse_len);
            free(sse_buffer);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "send_logs_to_clients_task: httpd_resp_send_chunk failed with code: %d[%s]", err,
                         esp_err_to_name(err));
                xSemaphoreGive(client_semaphores[i]);
                err = logger_remove_client(clients[i]);
                if (err != ESP_OK)
                    ESP_LOGE(TAG, "send_logs_to_clients_task: logger_remove_client failed with code: %d[%s]", err,
                             esp_err_to_name(err));
                continue;
            }

            xSemaphoreGive(client_semaphores[i]);
        }

        vRingbufferReturnItem(log_buffer, buffer);
    }
}

void logger_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "logger_init: start");
    for (int i = 0; i < CONFIG_LOG_MAX_SOCKETS; i++)
        client_semaphores[i] = xSemaphoreCreateMutex();
    log_buffer = xRingbufferCreate(CONFIG_LOG_BUFFER_SIZE_BYTES, RINGBUF_TYPE_BYTEBUF);
    esp_log_set_vprintf(logger_vprintf);
    xTaskCreate(send_logs_to_clients_task, "send_logs_to_clients_task", CONFIG_LOG_SEND_TASK_STACK_SIZE, NULL,
                CONFIG_LOG_SEND_TASK_PRIORITY, NULL);
}
