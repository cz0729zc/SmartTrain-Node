#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 CO2 采集模块
 *
 * 初始化 CO2 传感器 (JW01) 并创建采集任务。
 *
 * @return esp_err_t ESP_OK 成功，其他失败
 */
esp_err_t app_co2_start(void);

/**
 * @brief 停止 CO2 采集模块
 *
 * @return esp_err_t ESP_OK 成功，其他失败
 */
esp_err_t app_co2_stop(void);

/**
 * @brief 获取最新 CO2 浓度值
 *
 * @param co2_ppm 输出: CO2 浓度 (ppm)
 * @return esp_err_t ESP_OK 成功，ESP_ERR_NOT_FOUND 无有效数据
 */
esp_err_t app_co2_get_value(uint16_t *co2_ppm);

#ifdef __cplusplus
}
#endif
