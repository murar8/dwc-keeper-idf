#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "lwip/sys.h"
#include <string.h>

// tcp send
#include <lwip/sockets.h>

#define CONFIG_LOG_BUFFER_SIZE_BYTES 1024
#define CONFIG_LOG_MAX_SOCKETS 3

#define CONFIG_LOG_MAX_WAIT_TIME_MS 100

#define CONFIG_LOG_SEND_TASK_STACK_SIZE 2048
#define CONFIG_LOG_SEND_TASK_PRIORITY 0

static const char *TAG = "logger";

static RingbufHandle_t log_buffer;

static int socket_count = 0;
static int sockets[CONFIG_LOG_MAX_SOCKETS];
static SemaphoreHandle_t socket_semaphore;

static int logger_vprintf(const char *fmt, va_list args)
{

    int len = vprintf(fmt, args);
    char *buffer = malloc(len + 1);
    vsnprintf(buffer, len + 1, fmt, args);
    int sent = xRingbufferSend(log_buffer, buffer, len + 1, 0);
    if (sent == pdFALSE)
        free(buffer);
    return len;
}

esp_err_t logger_add_socket(int socket)
{
    int ret = xSemaphoreTake(socket_semaphore, pdMS_TO_TICKS(100));
    if (ret == pdFALSE)
    {
        ESP_LOGE(TAG, "logger_add_socket: xSemaphoreTake failed");
        return ESP_FAIL;
    }

    int socket_index = -1;
    for (int i = 0; i < CONFIG_LOG_MAX_SOCKETS; i++)
    {
        if (sockets[i] == -1)
        {
            socket_index = i;
            break;
        }
    }

    if (socket_index == -1)
    {
        ret = xSemaphoreGive(socket_semaphore);
        if (ret == pdFALSE)
        {
            ESP_LOGE(TAG, "logger_add_socket: xSemaphoreGive failed");
        }
        ESP_LOGE(TAG, "logger_add_socket: No free socket index found");
        return ESP_ERR_NO_MEM;
    }

    sockets[socket_index] = socket;
    socket_count++;

    ret = xSemaphoreGive(socket_semaphore);
    if (ret == pdFALSE)
    {
        ESP_LOGE(TAG, "logger_add_socket: xSemaphoreGive failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "logger_add_socket: socket %d added", socket);
    return ESP_OK;
}

esp_err_t logger_remove_socket(int socket)
{
    int ret = xSemaphoreTake(socket_semaphore, pdMS_TO_TICKS(100));
    if (ret == pdFALSE)
    {
        ESP_LOGE(TAG, "logger_remove_socket: xSemaphoreTake failed");
        return ESP_FAIL;
    }

    int socket_index = -1;
    for (int i = 0; i < CONFIG_LOG_MAX_SOCKETS; i++)
    {
        if (sockets[i] == socket)
        {
            socket_index = i;
            break;
        }
    }
    if (socket_index == -1)
    {
        ret = xSemaphoreGive(socket_semaphore);
        if (ret == pdFALSE)
            ESP_LOGE(TAG, "logger_remove_socket: xSemaphoreGive failed");
        return ESP_ERR_NOT_FOUND;
    }

    sockets[socket_index] = -1;
    socket_count--;

    ret = xSemaphoreGive(socket_semaphore);
    if (ret == pdFALSE)
    {
        ESP_LOGE(TAG, "logger_remove_socket: xSemaphoreGive failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void send_logs_to_sockets_task(void *arg)
{
    while (1)
    {
        size_t size;
        char *buffer = xRingbufferReceive(log_buffer, &size, pdMS_TO_TICKS(CONFIG_LOG_MAX_WAIT_TIME_MS));
        if (buffer == NULL)
            continue;

        int ret = xSemaphoreTake(socket_semaphore, portMAX_DELAY);
        if (ret == pdFALSE)
        {
            ESP_LOGE(TAG, "send_logs_to_sockets_task: xSemaphoreTake failed");
            continue;
        }

        for (int i = 0; i < CONFIG_LOG_MAX_SOCKETS; i++)
        {
            if (sockets[i] == -1)
                continue;
            send(sockets[i], buffer, size, 0);
        }

        ret = xSemaphoreGive(socket_semaphore);
        if (ret == pdFALSE)
        {
            ESP_LOGE(TAG, "send_logs_to_sockets_task: xSemaphoreGive failed");
        }
    }
}

void logger_init(void)
{
    ESP_LOGI(TAG, "logger_init: start");
    socket_semaphore = xSemaphoreCreateMutex();
    memset(sockets, -1, sizeof(sockets));
    log_buffer = xRingbufferCreate(CONFIG_LOG_BUFFER_SIZE_BYTES, RINGBUF_TYPE_NOSPLIT);
    esp_log_set_vprintf(logger_vprintf);
    xTaskCreate(send_logs_to_sockets_task, "send_logs_to_sockets_task", CONFIG_LOG_SEND_TASK_STACK_SIZE, NULL,
                CONFIG_LOG_SEND_TASK_PRIORITY, NULL);
}
