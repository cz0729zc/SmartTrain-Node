#pragma once

#include "esp_err.h"

/**
 * @brief 初始化 WiFi Station 模式
 * 
 * 启动 WiFi，注册事件处理程序，并发起连接。
 */
void app_wifi_init(void);

/**
 * @brief 等待 WiFi 连接成功
 * 
 * 阻塞调用，直到连接成功或达到最大重试次数失败。
 * 
 * @return esp_err_t ESP_OK 表示连接成功，ESP_FAIL 表示失败
 */
esp_err_t app_wifi_wait_connected(void);
