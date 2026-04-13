#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 执行一次 SNTP 时间同步
 *
 * 需要在网络已连接后调用。同步成功后会停止 SNTP，避免周期性请求。
 *
 * @param timeout_ms 等待同步超时（毫秒），传 0 使用 menuconfig 默认值
 * @return esp_err_t ESP_OK 成功，ESP_ERR_TIMEOUT 超时，其他为失败
 */
esp_err_t app_time_sync_once(uint32_t timeout_ms);

/**
 * @brief 当前时间是否已同步
 */
bool app_time_is_synchronized(void);

/**
 * @brief 获取当前 Unix 时间戳（秒）
 */
time_t app_time_now(void);

/**
 * @brief 获取当前本地时间字符串
 *
 * 格式：YYYY-MM-DD HH:MM:SS
 */
esp_err_t app_time_format_local(char *buf, size_t len);

#ifdef __cplusplus
}
#endif
