#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CO2 传感器配置结构
 */
typedef struct {
    gpio_num_t tx_gpio;     // UART TX 引脚
    gpio_num_t rx_gpio;     // UART RX 引脚
    int uart_num;           // UART 端口号 (UART_NUM_0/1/2)
} driver_co2_config_t;

/**
 * @brief 初始化 CO2 传感器 (JW01)
 *
 * 配置 UART 为 9600 波特率，8N1 格式。
 *
 * @param config 配置参数
 * @return esp_err_t ESP_OK 成功，其他失败
 */
esp_err_t driver_co2_init(const driver_co2_config_t *config);

/**
 * @brief 反初始化 CO2 传感器
 *
 * @return esp_err_t ESP_OK 成功，其他失败
 */
esp_err_t driver_co2_deinit(void);

/**
 * @brief 读取 CO2 浓度
 *
 * 从 UART 接收缓冲区解析最新的 CO2 数据。
 * 非阻塞，如果没有新数据返回 ESP_ERR_NOT_FOUND。
 *
 * @param co2_ppm 输出: CO2 浓度 (ppm)
 * @return esp_err_t ESP_OK 成功读取，ESP_ERR_NOT_FOUND 无新数据，其他为错误
 */
esp_err_t driver_co2_read(uint16_t *co2_ppm);

/**
 * @brief 阻塞读取 CO2 浓度
 *
 * 等待直到收到有效数据或超时。
 *
 * @param co2_ppm 输出: CO2 浓度 (ppm)
 * @param timeout_ms 超时时间 (毫秒)
 * @return esp_err_t ESP_OK 成功，ESP_ERR_TIMEOUT 超时，其他为错误
 */
esp_err_t driver_co2_read_timeout(uint16_t *co2_ppm, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
