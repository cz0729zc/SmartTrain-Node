/**
 * @file app_lvgl.h
 * @brief LVGL 应用层初始化
 */

#ifndef APP_LVGL_H
#define APP_LVGL_H

#include <esp_err.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL (包括 LCD 和触摸)
 * @return ESP_OK 成功
 */
esp_err_t app_lvgl_init(void);

/**
 * @brief 获取 LVGL 显示对象
 * @return lv_display_t 指针
 */
lv_display_t *app_lvgl_get_display(void);

/**
 * @brief 显示测试界面
 */
void app_lvgl_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_LVGL_H */
