#pragma once

#include "data_queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 传感器数据结构
 */
typedef struct {
    float temperature;  // 温度 (摄氏度)
    float humidity;     // 湿度 (百分比)
} sensor_data_t;

/**
 * @brief 初始化传感器数据队列
 *
 * @param queue_len 队列长度
 * @return esp_err_t ESP_OK 成功，其他失败
 */
esp_err_t sensor_queue_init(size_t queue_len);

/**
 * @brief 发送传感器数据到队列
 *
 * @param data 传感器数据指针
 * @param timeout_ms 超时时间 (毫秒)
 * @return esp_err_t ESP_OK 成功，ESP_ERR_TIMEOUT 超时
 */
esp_err_t sensor_queue_send(const sensor_data_t *data, uint32_t timeout_ms);

/**
 * @brief 从队列接收传感器数据
 *
 * @param data 传感器数据指针 (输出)
 * @param timeout_ms 超时时间 (毫秒)
 * @return esp_err_t ESP_OK 成功，ESP_ERR_TIMEOUT 超时
 */
esp_err_t sensor_queue_receive(sensor_data_t *data, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
