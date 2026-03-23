#include "data_queue.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "data_queue";

struct data_queue {
    QueueHandle_t queue;
    size_t item_size;
};

data_queue_handle_t data_queue_create(size_t item_size, size_t queue_len)
{
    if (item_size == 0 || queue_len == 0) {
        ESP_LOGE(TAG, "参数无效: item_size=%u, queue_len=%u", item_size, queue_len);
        return NULL;
    }

    data_queue_handle_t handle = malloc(sizeof(struct data_queue));
    if (handle == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return NULL;
    }

    handle->queue = xQueueCreate(queue_len, item_size);
    if (handle->queue == NULL) {
        ESP_LOGE(TAG, "创建 FreeRTOS 队列失败");
        free(handle);
        return NULL;
    }

    handle->item_size = item_size;
    ESP_LOGD(TAG, "队列创建成功: item_size=%u, queue_len=%u", item_size, queue_len);
    return handle;
}

void data_queue_destroy(data_queue_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    if (handle->queue != NULL) {
        vQueueDelete(handle->queue);
    }
    free(handle);
}

esp_err_t data_queue_send(data_queue_handle_t handle, const void *data, uint32_t timeout_ms)
{
    if (handle == NULL || handle->queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xQueueSend(handle->queue, data, ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t data_queue_receive(data_queue_handle_t handle, void *data, uint32_t timeout_ms)
{
    if (handle == NULL || handle->queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xQueueReceive(handle->queue, data, ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

size_t data_queue_count(data_queue_handle_t handle)
{
    if (handle == NULL || handle->queue == NULL) {
        return 0;
    }

    return uxQueueMessagesWaiting(handle->queue);
}
