#include <errno.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>
#include <sys/socket.h>

#include "logger.h"

#define CONFIG_LOGGER_MAX_CLIENTS 3

#define CONFIG_LOGGER_SEND_TO_CLIENTS_TASK_PRIORITY 10
#define CONFIG_LOGGER_SEND_TO_CLIENTS_TASK_STACK_SIZE 2048

#define CONFIG_LOGGER_QUEUE_SIZE 50

static const char *TAG = "logger";

static QueueHandle_t log_queue_handle;

static httpd_req_t *clients[CONFIG_LOGGER_MAX_CLIENTS];
static int client_sockfds[CONFIG_LOGGER_MAX_CLIENTS]; // Track socket FDs for cleanup
static SemaphoreHandle_t client_mutexes[CONFIG_LOGGER_MAX_CLIENTS];

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

static bool is_socket_valid(httpd_req_t *req)
{
    int sockfd = httpd_req_to_sockfd(req);
    if (sockfd < 0)
    {
        return false;
    }

    // Use recv with MSG_PEEK to check if socket is still connected
    char dummy;
    int ret = recv(sockfd, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);

    // If recv returns 0, the socket is closed
    // If recv returns -1 with EWOULDBLOCK, socket is open but no data
    // Any other error means socket is invalid
    if (ret == 0 || (ret < 0 && errno != EWOULDBLOCK && errno != EAGAIN))
    {
        return false;
    }

    return true;
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
                if (xSemaphoreTake(client_mutexes[i], 0) == pdTRUE) // Non-blocking take
                {
                    if (clients[i] != NULL)
                    {
                        // Check if socket is still valid before sending
                        if (!is_socket_valid(clients[i]))
                        {
                            ESP_LOGW(TAG, "send_to_clients: Client %d socket closed, removing", i);
                            httpd_req_async_handler_complete(clients[i]);
                            clients[i] = NULL;
                            xSemaphoreGive(client_mutexes[i]);
                            continue;
                        }

                        esp_err_t err = httpd_resp_send_chunk(clients[i], buffer, HTTPD_RESP_USE_STRLEN);
                        if (err != ESP_OK)
                        {
                            ESP_LOGW(TAG, "send_to_clients: Failed to send to client %d, removing", i);
                            httpd_req_async_handler_complete(clients[i]);
                            clients[i] = NULL;
                            client_sockfds[i] = -1;
                        }
                    }
                    xSemaphoreGive(client_mutexes[i]);
                }
            }
            free(buffer);
        }
    }
}

void logger_init()
{
    ESP_LOGI(TAG, "logger_init: Initializing logger");
    assert(log_queue_handle == NULL);
    assert(original_vprintf == NULL);

    log_queue_handle = xQueueCreate(CONFIG_LOGGER_QUEUE_SIZE, sizeof(char *));
    original_vprintf = esp_log_set_vprintf(logger_vprintf);
    assert(log_queue_handle != NULL);

    // Initialize mutexes and sockfds for each client slot
    for (int i = 0; i < CONFIG_LOGGER_MAX_CLIENTS; i++)
    {
        client_mutexes[i] = xSemaphoreCreateMutex();
        client_sockfds[i] = -1; // Invalid sockfd
        assert(client_mutexes[i] != NULL);
    }

    xTaskCreate(send_to_clients, "send_to_clients", CONFIG_LOGGER_SEND_TO_CLIENTS_TASK_STACK_SIZE, NULL,
                CONFIG_LOGGER_SEND_TO_CLIENTS_TASK_PRIORITY, NULL);
    ESP_LOGI(TAG, "logger_init: Logger initialized");
}

esp_err_t logger_add_client(httpd_req_t *req)
{
    for (int i = 0; i < CONFIG_LOGGER_MAX_CLIENTS; i++)
    {
        if (xSemaphoreTake(client_mutexes[i], pdMS_TO_TICKS(1000)) == pdFALSE)
        {
            continue;
        }
        if (clients[i] != NULL)
        {
            xSemaphoreGive(client_mutexes[i]);
            continue;
        }
        esp_err_t ret = httpd_req_async_handler_begin(req, &clients[i]);
        if (ret == ESP_OK)
        {
            client_sockfds[i] = httpd_req_to_sockfd(clients[i]);
            xSemaphoreGive(client_mutexes[i]);
            ESP_LOGI(TAG, "logger_add_client: Added client %d with sockfd %d", i, client_sockfds[i]);
            return ESP_OK;
        }
        else
        {
            xSemaphoreGive(client_mutexes[i]);
            ESP_LOGE(TAG, "logger_add_client: httpd_req_async_handler_begin failed with code: %d[%s]", ret,
                     esp_err_to_name(ret));
            return ret;
        }
    }
    {
        ESP_LOGE(TAG, "logger_add_client: Max clients reached");
        return ESP_FAIL;
    }
}

void logger_remove_client_by_sockfd(int sockfd)
{
    for (int i = 0; i < CONFIG_LOGGER_MAX_CLIENTS; i++)
    {
        if (xSemaphoreTake(client_mutexes[i], pdMS_TO_TICKS(100)) == pdFALSE)
        {
            continue;
        }
        if (client_sockfds[i] != sockfd)
        {
            xSemaphoreGive(client_mutexes[i]);
            continue;
        }
        httpd_req_async_handler_complete(clients[i]);
        clients[i] = NULL;
        client_sockfds[i] = -1;
        xSemaphoreGive(client_mutexes[i]);
        ESP_LOGI(TAG, "logger_remove_client_by_sockfd: Removed client %d with sockfd %d", i, sockfd);
        return;
    }
}
