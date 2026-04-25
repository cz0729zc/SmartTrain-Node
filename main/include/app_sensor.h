#pragma once

#include "esp_err.h"

/**
 * @brief 启动传感器采集任务
 * 
 * 创建一个独立的 FreeRTOS 任务，定期从传感器读取数据。
 * 
 * @return esp_err_t ESP_OK 表示成功，其他表示失败
 */
esp_err_t app_sensor_start(void);

/**
 * @brief 执行 DHT11 单次探测（用于开机自检）
 *
 * @param humidity 输出湿度（可为 NULL）
 * @param temperature 输出温度（可为 NULL）
 * @return esp_err_t ESP_OK 表示探测成功，其他表示失败
 */
esp_err_t app_sensor_probe(float *humidity, float *temperature);
