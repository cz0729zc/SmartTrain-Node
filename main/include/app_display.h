/**
 * @file app_display.h
 * @brief 显示模块应用层 (测试阶段)
 */

#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include <esp_err.h>
#include <stdint.h>

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

/* 启动自检文本页的基础颜色（RGB565） */
#define APP_DISPLAY_COLOR_BLACK   0x0000
#define APP_DISPLAY_COLOR_WHITE   0xFFFF
#define APP_DISPLAY_COLOR_GREEN   0x07E0
#define APP_DISPLAY_COLOR_RED     0xF800
#define APP_DISPLAY_COLOR_YELLOW  0xFFE0

/**
 * @brief 初始化启动文本页所需的 LCD（不依赖 LVGL）
 *
 * 仅初始化 LCD 并打开背光，适用于系统早期自检信息显示。
 */
esp_err_t app_display_boot_init(void);

/**
 * @brief 清屏并显示标题
 *
 * @param title 标题（建议英文大写，5x7 字模）
 */
void app_display_boot_show_title(const char *title);

/**
 * @brief 在指定行显示文本
 *
 * @param line_index 行号（从 0 开始）
 * @param text 文本（ASCII）
 * @param color RGB565 颜色
 */
void app_display_boot_show_line(uint8_t line_index, const char *text, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* APP_DISPLAY_H */
