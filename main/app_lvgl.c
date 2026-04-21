/**
 * @file app_lvgl.c
 * @brief LVGL 应用层初始化实现
 */

#include "app_lvgl.h"
#include "bsp_lcd.h"
#include "bsp_touch.h"
#include "bsp_pin_defs.h"

#include <esp_log.h>
#include <esp_lvgl_port.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "app_lvgl";

/* LVGL 显示和输入设备 */
static lv_display_t *s_lvgl_disp = NULL;
static lv_indev_t *s_lvgl_touch = NULL;
static SemaphoreHandle_t s_method_sem = NULL;
static app_lvgl_checkin_method_t s_selected_method = APP_LVGL_CHECKIN_METHOD_NONE;
static lv_obj_t *s_btn_face = NULL;
static lv_obj_t *s_btn_finger = NULL;

/* 绘图缓冲区高度 (行数) */
#define LVGL_DRAW_BUF_LINES     40

static void ui_clear_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A1F33), LV_PART_MAIN);
}

static lv_obj_t *ui_create_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, text != NULL ? text : "");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF8FAFC), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_source_han_sans_sc_16_cjk, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);
    return title;
}

static lv_obj_t *ui_create_line(lv_obj_t *parent, const char *text, lv_coord_t y, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_source_han_sans_sc_14_cjk, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static lv_obj_t *ui_create_action_button(lv_obj_t *parent,
                                         const char *text,
                                         lv_coord_t x,
                                         lv_coord_t y,
                                         lv_color_t bg)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 190, 80);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, x, y);
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 12, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_obj_set_style_text_font(label, &lv_font_source_han_sans_sc_16_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(label);
    return btn;
}

static void method_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);

    if (target == s_btn_face) {
        s_selected_method = APP_LVGL_CHECKIN_METHOD_FACE;
    } else if (target == s_btn_finger) {
        s_selected_method = APP_LVGL_CHECKIN_METHOD_FINGER;
    } else {
        s_selected_method = APP_LVGL_CHECKIN_METHOD_NONE;
    }

    if (s_method_sem != NULL && s_selected_method != APP_LVGL_CHECKIN_METHOD_NONE) {
        xSemaphoreGive(s_method_sem);
    }
}

esp_err_t app_lvgl_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "初始化 LVGL...");

    /* 1. 初始化 LCD 硬件 */
    ret = bsp_lcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD 初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 2. 初始化触摸屏硬件 */
    ret = bsp_touch_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "触摸屏初始化失败: %s (继续运行)", esp_err_to_name(ret));
        /* 触摸失败不影响显示，继续 */
    }

    /* 3. 初始化 LVGL port */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 6144,
        .task_affinity = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
    };
    ret = lvgl_port_init(&lvgl_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL port 初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 4. 添加 LCD 显示 */
    ESP_LOGI(TAG, "添加 LCD 显示 (%dx%d)", BSP_LCD_H_RES, BSP_LCD_V_RES);
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = bsp_lcd_get_io_handle(),
        .panel_handle = bsp_lcd_get_panel_handle(),
        .buffer_size = BSP_LCD_V_RES * LVGL_DRAW_BUF_LINES,
        .double_buffer = true,
        .hres = BSP_LCD_V_RES,
        .vres = BSP_LCD_H_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = false,  /* ILI9488 不需要交换字节 */
        },
    };
    s_lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    if (s_lvgl_disp == NULL) {
        ESP_LOGE(TAG, "添加 LVGL 显示失败");
        return ESP_FAIL;
    }

    /* 5. 添加触摸输入 (如果初始化成功) */
    esp_lcd_touch_handle_t touch_handle = bsp_touch_get_handle();
    if (touch_handle != NULL) {
        ESP_LOGI(TAG, "添加触摸输入");
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = s_lvgl_disp,
            .handle = touch_handle,
        };
        s_lvgl_touch = lvgl_port_add_touch(&touch_cfg);
        if (s_lvgl_touch == NULL) {
            ESP_LOGW(TAG, "添加 LVGL 触摸失败 (继续运行)");
        }
    }

    /* 6. 打开背光 */
    bsp_lcd_backlight_set(80);

    if (s_method_sem == NULL) {
        s_method_sem = xSemaphoreCreateBinary();
        if (s_method_sem == NULL) {
            ESP_LOGE(TAG, "创建方法选择信号量失败");
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(TAG, "LVGL 初始化完成");
    return ESP_OK;
}

lv_display_t *app_lvgl_get_display(void)
{
    return s_lvgl_disp;
}

void app_lvgl_demo(void)
{
    ESP_LOGI(TAG, "显示 LVGL 测试界面");

    /* 加锁 */
    lvgl_port_lock(0);

    /* 获取当前屏幕 */
    lv_obj_t *scr = lv_scr_act();

    /* 设置背景颜色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003a57), LV_PART_MAIN);

    /* 创建标题标签 */
    lv_obj_t *label_title = lv_label_create(scr);
    lv_label_set_text(label_title, "ESP32-S3 + LVGL");
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 30);

    /* 创建信息标签 */
    lv_obj_t *label_info = lv_label_create(scr);
    lv_label_set_text(label_info,
        "LCD: ILI9488 320x480\n"
        "Touch: XPT2046\n"
        "LVGL: v9.x");
    lv_obj_set_style_text_color(label_info, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_align(label_info, LV_ALIGN_CENTER, 0, -30);

    /* 创建一个按钮 */
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 150, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -50);

    lv_obj_t *label_btn = lv_label_create(btn);
    lv_label_set_text(label_btn, "Touch Me!");
    lv_obj_center(label_btn);

    /* 解锁 */
    lvgl_port_unlock();
}

void app_lvgl_show_idle(const char *title, const char *line1, const char *line2)
{
    if (s_lvgl_disp == NULL) {
        return;
    }

    lvgl_port_lock(0);
    ui_clear_screen();
    lv_obj_t *scr = lv_scr_act();

    ui_create_title(scr, title != NULL ? title : "考勤就绪 ATTENDANCE READY");
    ui_create_line(scr, line1 != NULL ? line1 : "请刷 RFID 卡  Tap your RFID card", 95, lv_color_hex(0xD1D5DB));
    ui_create_line(scr, line2 != NULL ? line2 : "人脸或指纹  Face or Fingerprint", 132, lv_color_hex(0x93C5FD));

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "系统在线  System online");
    lv_obj_set_style_text_font(hint, &lv_font_source_han_sans_sc_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x34D399), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);
    lvgl_port_unlock();
}

void app_lvgl_show_method_select(const char *student_id, const char *name)
{
    if (s_lvgl_disp == NULL) {
        return;
    }

    lvgl_port_lock(0);
    ui_clear_screen();
    lv_obj_t *scr = lv_scr_act();
    s_selected_method = APP_LVGL_CHECKIN_METHOD_NONE;

    char info[96] = {0};
    lv_snprintf(info, sizeof(info), "学号 ID:%s  姓名 NAME:%s",
                student_id != NULL ? student_id : "-",
                name != NULL ? name : "-");

    ui_create_title(scr, "请选择打卡方式");
    ui_create_line(scr, info, 70, lv_color_hex(0xE2E8F0));
    ui_create_line(scr, "二选一即可通过", 98, lv_color_hex(0x94A3B8));

    s_btn_face = ui_create_action_button(scr, "人脸 Face", -105, 145, lv_color_hex(0x2563EB));
    s_btn_finger = ui_create_action_button(scr, "指纹 Finger", 105, 145, lv_color_hex(0x059669));

    lv_obj_add_event_cb(s_btn_face, method_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_btn_finger, method_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lvgl_port_unlock();
}

esp_err_t app_lvgl_wait_method_select(uint32_t timeout_ms, app_lvgl_checkin_method_t *out_method)
{
    if (out_method == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_method_sem == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    while (xSemaphoreTake(s_method_sem, 0) == pdTRUE) {
        // Drain stale signals from previous selection.
    }

    if (xSemaphoreTake(s_method_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    *out_method = s_selected_method;
    return (*out_method == APP_LVGL_CHECKIN_METHOD_NONE) ? ESP_FAIL : ESP_OK;
}

void app_lvgl_show_processing(const char *title, const char *line)
{
    if (s_lvgl_disp == NULL) {
        return;
    }

    lvgl_port_lock(0);
    ui_clear_screen();
    lv_obj_t *scr = lv_scr_act();

    ui_create_title(scr, title != NULL ? title : "处理中 Processing");
    ui_create_line(scr, line != NULL ? line : "请稍候...", 110, lv_color_hex(0xF8FAFC));

    lv_obj_t *spinner = lv_spinner_create(scr);
    lv_obj_set_size(spinner, 60, 60);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 36);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);

    lvgl_port_unlock();
}

void app_lvgl_show_result(bool success, const char *line1, const char *line2)
{
    if (s_lvgl_disp == NULL) {
        return;
    }

    lvgl_port_lock(0);
    ui_clear_screen();
    lv_obj_t *scr = lv_scr_act();

    lv_color_t status_color = success ? lv_color_hex(0x10B981) : lv_color_hex(0xEF4444);
    ui_create_title(scr, success ? "打卡成功" : "打卡失败");
    ui_create_line(scr, line1 != NULL ? line1 : "", 100, status_color);
    ui_create_line(scr, line2 != NULL ? line2 : "", 132, lv_color_hex(0xCBD5E1));
    lvgl_port_unlock();
}
