#include "driver_co2.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "driver_co2";

/* JW01 协议常量 */
#define CO2_FRAME_HEADER    0x2C
#define CO2_FRAME_LENGTH    6
#define CO2_UART_BUF_SIZE   256

/* 模块状态 */
static int s_uart_num = -1;
static bool s_initialized = false;

/**
 * @brief 验证数据帧校验和
 */
static bool verify_checksum(const uint8_t *data)
{
    uint8_t sum = data[0] + data[1] + data[2] + data[3] + data[4];
    return (sum == data[5]);
}

/**
 * @brief 从缓冲区解析 CO2 数据帧
 */
static esp_err_t parse_co2_frame(const uint8_t *buf, size_t len, uint16_t *co2_ppm)
{
    // 查找帧头 0x2C
    for (size_t i = 0; i + CO2_FRAME_LENGTH <= len; i++) {
        if (buf[i] == CO2_FRAME_HEADER) {
            // 验证校验和
            if (verify_checksum(&buf[i])) {
                // 解析 CO2 值: Byte[1] * 256 + Byte[2]
                *co2_ppm = (uint16_t)buf[i + 1] * 256 + buf[i + 2];
                return ESP_OK;
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t driver_co2_init(const driver_co2_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_initialized) {
        ESP_LOGW(TAG, "CO2 驱动已初始化");
        return ESP_ERR_INVALID_STATE;
    }

    // UART 配置: 9600 8N1
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(config->uart_num, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 参数配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(config->uart_num, config->tx_gpio, config->rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 引脚配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_driver_install(config->uart_num, CO2_UART_BUF_SIZE, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    s_uart_num = config->uart_num;
    s_initialized = true;

    ESP_LOGI(TAG, "CO2 传感器初始化成功 (UART%d, TX:%d, RX:%d)",
             config->uart_num, config->tx_gpio, config->rx_gpio);

    return ESP_OK;
}

esp_err_t driver_co2_deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = uart_driver_delete(s_uart_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 驱动卸载失败: %s", esp_err_to_name(ret));
        return ret;
    }

    s_uart_num = -1;
    s_initialized = false;

    ESP_LOGI(TAG, "CO2 传感器已关闭");
    return ESP_OK;
}

esp_err_t driver_co2_read(uint16_t *co2_ppm)
{
    if (co2_ppm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[CO2_UART_BUF_SIZE];

    // 读取 UART 缓冲区中的所有数据
    int len = uart_read_bytes(s_uart_num, buf, sizeof(buf), 0);
    if (len <= 0) {
        return ESP_ERR_NOT_FOUND;
    }

    // 解析最后一个有效帧
    return parse_co2_frame(buf, len, co2_ppm);
}

esp_err_t driver_co2_read_timeout(uint16_t *co2_ppm, uint32_t timeout_ms)
{
    if (co2_ppm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[CO2_FRAME_LENGTH * 2];  // 读取足够容纳一个完整帧
    size_t total_len = 0;
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    // 清空接收缓冲区
    uart_flush_input(s_uart_num);

    while ((xTaskGetTickCount() - start_tick) < timeout_ticks) {
        int len = uart_read_bytes(s_uart_num, buf + total_len,
                                   sizeof(buf) - total_len,
                                   pdMS_TO_TICKS(100));
        if (len > 0) {
            total_len += len;

            // 尝试解析
            if (total_len >= CO2_FRAME_LENGTH) {
                if (parse_co2_frame(buf, total_len, co2_ppm) == ESP_OK) {
                    return ESP_OK;
                }
            }

            // 缓冲区满但未找到有效帧，移动数据
            if (total_len >= sizeof(buf)) {
                memmove(buf, buf + CO2_FRAME_LENGTH, CO2_FRAME_LENGTH);
                total_len = CO2_FRAME_LENGTH;
            }
        }
    }

    return ESP_ERR_TIMEOUT;
}
