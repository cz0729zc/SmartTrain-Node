/**
 * @file bsp_touch.h
 * @brief BSP 触摸屏驱动封装 (XPT2046)
 */

#ifndef BSP_TOUCH_H
#define BSP_TOUCH_H

#include <esp_err.h>
#include <esp_lcd_touch.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化触摸屏
 * @note 必须在 bsp_lcd_init() 之后调用 (共用 SPI 总线)
 * @return ESP_OK 成功
 */
esp_err_t bsp_touch_init(void);

/**
 * @brief 读取触摸数据
 * @param[out] x X坐标 (0 ~ LCD_H_RES-1)
 * @param[out] y Y坐标 (0 ~ LCD_V_RES-1)
 * @return true 有触摸, false 无触摸
 */
bool bsp_touch_read(uint16_t *x, uint16_t *y);

/**
 * @brief 获取触摸屏 handle
 * @return touch handle, 用于 LVGL 等库
 */
esp_lcd_touch_handle_t bsp_touch_get_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TOUCH_H */
