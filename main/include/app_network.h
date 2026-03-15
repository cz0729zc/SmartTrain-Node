#pragma once

#include "esp_err.h"

/**
 * @brief 启动网络管理任务
 * 
 * 创建一个独立的 FreeRTOS 任务，负责：
 * 1. 初始化并连接 WiFi (通过调用 components/app_wifi)
 * 2. 维护网络状态（断线重连）
 * 3. 启动并管理 MQTT 连接 (后续实现)
 * 
 * @return esp_err_t ESP_OK 表示成功，其他表示失败
 */
esp_err_t app_network_start(void);
