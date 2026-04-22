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

typedef enum {
	APP_LVGL_PAGE_BOOT_SELFCHECK = 0,
	APP_LVGL_PAGE_STANDBY,
	APP_LVGL_PAGE_USER_CONFIRM,
	APP_LVGL_PAGE_FACE_PUNCH,
	APP_LVGL_PAGE_FINGER_PUNCH,
	APP_LVGL_PAGE_RESULT,
	APP_LVGL_PAGE_UNREGISTERED,
	APP_LVGL_PAGE_COUNT,
} app_lvgl_page_t;

typedef enum {
	APP_LVGL_EVT_SELFCHECK_DONE = 0,
	APP_LVGL_EVT_CARD_DETECTED,
	APP_LVGL_EVT_SELECT_FACE,
	APP_LVGL_EVT_SELECT_FINGER,
	APP_LVGL_EVT_FACE_OK,
	APP_LVGL_EVT_FACE_FAIL,
	APP_LVGL_EVT_FINGER_OK,
	APP_LVGL_EVT_FINGER_FAIL,
	APP_LVGL_EVT_CARD_UNREGISTERED,
	APP_LVGL_EVT_CARD_UNBOUND,
	APP_LVGL_EVT_BACK,
	APP_LVGL_EVT_CANCEL,
	APP_LVGL_EVT_HOME,
	APP_LVGL_EVT_TIMEOUT,
} app_lvgl_event_t;

typedef void (*app_lvgl_action_cb_t)(app_lvgl_event_t event, void *arg);

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

/**
 * @brief 触发 UI 状态机事件（线程安全）
 */
void app_lvgl_dispatch_event(app_lvgl_event_t event);

/**
 * @brief 更新当前用户信息（用于确认页/结果页）
 */
void app_lvgl_update_user_info(const char *student_id, const char *name, const char *uid_short);

/**
 * @brief 更新结果页信息
 */
void app_lvgl_update_result(bool success, const char *method, const char *reason, const char *time_text);

/**
 * @brief 设置 UI 按钮动作回调（可选）
 */
void app_lvgl_set_action_callback(app_lvgl_action_cb_t callback, void *arg);

/**
 * @brief 启动 7 页面轮播测试（用于 UI 观感巡检）
 * @param interval_ms 页面切换周期，单位毫秒
 */
void app_lvgl_start_cycle_test(uint32_t interval_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_LVGL_H */
