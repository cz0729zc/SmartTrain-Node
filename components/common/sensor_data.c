#include "sensor_data.h"
#include "esp_log.h"

static const char *TAG = "sensor_queue";

static QueueHandle_t s_sensor_queue = NULL;

esp_err_t sensor_queue_init(uint8_t queue_size)
{
    if (s_sensor_queue != NULL) {
        ESP_LOGW(TAG, "队列已初始化");
        return ESP_ERR_INVALID_STATE;
    }

    s_sensor_queue = xQueueCreate(queue_size, sizeof(sensor_data_t));
    if (s_sensor_queue == NULL) {
        ESP_LOGE(TAG, "创建传感器队列失败");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "传感器队列初始化成功, 容量=%d", queue_size);
    return ESP_OK;
}

esp_err_t sensor_queue_send(const sensor_data_t *data, uint32_t timeout_ms)
{
    if (s_sensor_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xQueueSend(s_sensor_queue, data, ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t sensor_queue_receive(sensor_data_t *data, uint32_t timeout_ms)
{
    if (s_sensor_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xQueueReceive(s_sensor_queue, data, ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}
