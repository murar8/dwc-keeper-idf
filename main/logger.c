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

static int vprintf_failed_allocations = 0;
static int vprintf_failed_sends = 0;

static int logger_vprintf(const char *fmt, va_list args)
{

    va_list args_copy;
    va_copy(args_copy, args);
    int len = vprintf(fmt, args_copy);
    va_end(args_copy);

    if (len < 0)
        return len;

    static char stack_buffer[CONFIG_LOG_STACK_BUFFER_SIZE_BYTES];
    char *buffer = stack_buffer;
    if (len + 1 > CONFIG_LOG_STACK_BUFFER_SIZE_BYTES)
    {
        buffer = malloc(len + 1);
        if (!buffer)
        {
            vprintf_failed_allocations++;
            return len;
        }
    }

    vsnprintf(buffer, len + 1, fmt, args);
    int ret = xRingbufferSend(log_buffer, buffer, len + 1, 0 /* no wait */);
    if (!ret)
        vprintf_failed_sends++;

    if (buffer != stack_buffer)
        free(buffer);

    return len;
}

esp_err_t logger_add_client(httpd_req_t *in_req)
{
    httpd_req_t *req;
    esp_err_t ret = httpd_req_async_handler_begin(in_req, &req);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "logger_add_client: httpd_req_async_handler_begin failed with code: %d[%s]", ret,
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(httpd_resp_set_type(req, "text/event-stream"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Cache-Control", "no-cache"));
    ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "Connection", "keep-alive"));

    ret = httpd_resp_send_chunk(req, "data: connected\n\n", 17);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "logger_add_client: httpd_resp_send_chunk failed with code: %d[%s]", ret, esp_err_to_name(ret));
        httpd_req_async_handler_complete(req);
        return ret;
    }

    for (int i = 0;; i++)
    {
        if (i == CONFIG_LOG_MAX_SOCKETS)
        {
            ESP_LOGE(TAG, "logger_add_client: No free client index found");
            httpd_resp_send_custom_err(req, "503", "Too many log connections");
            httpd_req_async_handler_complete(req);
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
        esp_err_t err = httpd_req_async_handler_complete(req);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "logger_remove_client: httpd_req_async_handler_complete failed with code: %d[%s]", err,
                     esp_err_to_name(err));
        ESP_LOGI(TAG, "logger_remove_client: client %d removed", i);
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

        for (int i = 0;; i++)
        {
            if (i == CONFIG_LOG_MAX_SOCKETS)
            {
                vRingbufferReturnItem(log_buffer, buffer);
                break;
            }

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
            else
            {
                xSemaphoreGive(client_semaphores[i]);
            }
        }
    }
}

static void print_vprintf_stats_task(void *arg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(30000));
        if (vprintf_failed_allocations > 0)
        {
            vprintf_failed_allocations = 0;
            ESP_LOGE(TAG, "print_vprintf_stats_task: Failed to allocate %d times", vprintf_failed_allocations);
        }
        if (vprintf_failed_sends > 0)
        {
            vprintf_failed_sends = 0;
            ESP_LOGE(TAG, "print_vprintf_stats_task: Failed to send %d times", vprintf_failed_sends);
        }
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
    xTaskCreate(print_vprintf_stats_task, "print_vprintf_stats_task", CONFIG_ESP32_PTHREAD_STACK_MIN, NULL,
                CONFIG_LOG_SEND_TASK_PRIORITY, NULL);
}
