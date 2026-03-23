#include "sensor_data.h"
#include "esp_log.h"

static const char *TAG = "sensor_data";

static data_queue_handle_t s_sensor_queue = NULL;

esp_err_t sensor_queue_init(size_t queue_len)
{
    if (s_sensor_queue != NULL) {
        ESP_LOGW(TAG, "传感器队列已初始化");
        return ESP_ERR_INVALID_STATE;
    }

    s_sensor_queue = data_queue_create(sizeof(sensor_data_t), queue_len);
    if (s_sensor_queue == NULL) {
        ESP_LOGE(TAG, "创建传感器队列失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "传感器队列初始化成功, 容量=%u", queue_len);
    return ESP_OK;
}

esp_err_t sensor_queue_send(const sensor_data_t *data, uint32_t timeout_ms)
{
    if (s_sensor_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return data_queue_send(s_sensor_queue, data, timeout_ms);
}

esp_err_t sensor_queue_receive(sensor_data_t *data, uint32_t timeout_ms)
{
    if (s_sensor_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return data_queue_receive(s_sensor_queue, data, timeout_ms);
}
