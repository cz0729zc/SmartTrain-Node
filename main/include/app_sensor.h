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
