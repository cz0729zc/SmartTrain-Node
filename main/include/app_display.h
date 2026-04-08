/**
 * @file app_display.h
 * @brief 显示模块应用层 (测试阶段)
 */

#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化显示模块 (LCD + 触摸屏)
 * @return ESP_OK 成功
 */
esp_err_t app_display_init(void);

/**
 * @brief 运行显示测试
 *
 * 测试内容:
 * - 循环显示红、绿、蓝纯色
 * - 打印触摸坐标到串口
 */
void app_display_test(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DISPLAY_H */
