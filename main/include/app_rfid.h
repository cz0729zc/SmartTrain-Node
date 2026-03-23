#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RFID 卡片信息结构
 */
typedef struct {
    uint8_t uid[10];      // 卡片 UID (最大 10 字节)
    uint8_t uid_length;   // UID 长度 (4/7/10 字节)
    char uid_str[32];     // UID 字符串表示 (如 "04:A1:B2:C3")
} rfid_card_info_t;

/**
 * @brief RFID 卡片事件回调类型
 *
 * @param card_info 卡片信息
 * @param arg 用户参数
 */
typedef void (*app_rfid_card_cb_t)(const rfid_card_info_t *card_info, void *arg);

/**
 * @brief 启动 RFID 模块
 *
 * 初始化 RC522 读卡器 (I2C 模式) 并启动扫描任务。
 *
 * @return esp_err_t ESP_OK 成功，其他失败
 */
esp_err_t app_rfid_start(void);

/**
 * @brief 停止 RFID 模块
 *
 * @return esp_err_t ESP_OK 成功，其他失败
 */
esp_err_t app_rfid_stop(void);

/**
 * @brief 设置卡片检测回调
 *
 * 当检测到卡片时，调用此回调通知上层应用。
 *
 * @param callback 回调函数
 * @param arg 用户参数
 */
void app_rfid_set_card_callback(app_rfid_card_cb_t callback, void *arg);

/**
 * @brief 读取卡片数据块
 *
 * @param block_address 块地址 (0-63 for MIFARE 1K)
 * @param buffer 输出缓冲区 (16 字节)
 * @param buffer_size 缓冲区大小 (必须 >= 16)
 * @return esp_err_t ESP_OK 成功，其他失败
 */
esp_err_t app_rfid_read_block(uint8_t block_address, uint8_t *buffer, size_t buffer_size);

/**
 * @brief 写入卡片数据块
 *
 * @param block_address 块地址 (避免 Block 0 和 Sector Trailer)
 * @param data 写入数据 (16 字节)
 * @param data_len 数据长度 (必须 == 16)
 * @return esp_err_t ESP_OK 成功，其他失败
 */
esp_err_t app_rfid_write_block(uint8_t block_address, const uint8_t *data, size_t data_len);

#ifdef __cplusplus
}
#endif
