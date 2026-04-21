/**
 * @file app_lvgl.h
 * @brief LVGL 应用层初始化
 */

#ifndef APP_LVGL_H
#define APP_LVGL_H

#include <esp_err.h>
#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

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

typedef enum {
	APP_LVGL_CHECKIN_METHOD_NONE = 0,
	APP_LVGL_CHECKIN_METHOD_FACE,
	APP_LVGL_CHECKIN_METHOD_FINGER,
} app_lvgl_checkin_method_t;

/**
 * @brief 显示待机页
 */
void app_lvgl_show_idle(const char *title, const char *line1, const char *line2);

/**
 * @brief 显示二选一打卡页
 */
void app_lvgl_show_method_select(const char *student_id, const char *name);

/**
 * @brief 等待用户选择打卡方式
 * @param timeout_ms 等待超时(ms)
 * @param out_method 输出选择结果
 */
esp_err_t app_lvgl_wait_method_select(uint32_t timeout_ms, app_lvgl_checkin_method_t *out_method);

/**
 * @brief 显示处理中页面
 */
void app_lvgl_show_processing(const char *title, const char *line);

/**
 * @brief 显示结果页
 */
void app_lvgl_show_result(bool success, const char *line1, const char *line2);

#ifdef __cplusplus
}
#endif

#endif /* APP_LVGL_H */
