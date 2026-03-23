#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 数据队列句柄
 */
typedef struct data_queue* data_queue_handle_t;

/**
 * @brief 创建数据队列
 *
 * @param item_size 单个数据项的大小 (字节)
 * @param queue_len 队列长度 (可容纳的数据项数量)
 * @return data_queue_handle_t 队列句柄，失败返回 NULL
 */
data_queue_handle_t data_queue_create(size_t item_size, size_t queue_len);

/**
 * @brief 销毁数据队列
 *
 * @param handle 队列句柄
 */
void data_queue_destroy(data_queue_handle_t handle);

/**
 * @brief 发送数据到队列
 *
 * @param handle 队列句柄
 * @param data 数据指针
 * @param timeout_ms 超时时间 (毫秒)，0 表示不等待
 * @return esp_err_t ESP_OK 成功，ESP_ERR_TIMEOUT 超时，其他失败
 */
esp_err_t data_queue_send(data_queue_handle_t handle, const void *data, uint32_t timeout_ms);

/**
 * @brief 从队列接收数据
 *
 * @param handle 队列句柄
 * @param data 数据指针 (输出)
 * @param timeout_ms 超时时间 (毫秒)，portMAX_DELAY 表示永久等待
 * @return esp_err_t ESP_OK 成功，ESP_ERR_TIMEOUT 超时，其他失败
 */
esp_err_t data_queue_receive(data_queue_handle_t handle, void *data, uint32_t timeout_ms);

/**
 * @brief 获取队列中当前的数据项数量
 *
 * @param handle 队列句柄
 * @return size_t 数据项数量
 */
size_t data_queue_count(data_queue_handle_t handle);

#ifdef __cplusplus
}
#endif
