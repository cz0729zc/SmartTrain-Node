/**
 * @file bsp_lcd.h
 * @brief BSP LCD 驱动封装 (ILI9488)
 */

#ifndef BSP_LCD_H
#define BSP_LCD_H

#include <esp_err.h>
#include <esp_lcd_panel_ops.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LCD (包括 SPI 总线)
 * @return ESP_OK 成功
 */
esp_err_t bsp_lcd_init(void);

/**
 * @brief 设置背光亮度
 * @param percent 亮度百分比 (0-100)
 */
void bsp_lcd_backlight_set(uint8_t percent);

/**
 * @brief 获取 LCD panel handle
 * @return panel handle, 用于绘图操作
 */
esp_lcd_panel_handle_t bsp_lcd_get_panel_handle(void);

/**
 * @brief 填充纯色 (用于测试)
 * @param color RGB565 颜色值
 */
void bsp_lcd_fill_color(uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LCD_H */
