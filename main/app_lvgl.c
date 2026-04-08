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

static const char *TAG = "app_lvgl";

/* LVGL 显示和输入设备 */
static lv_display_t *s_lvgl_disp = NULL;
static lv_indev_t *s_lvgl_touch = NULL;

/* 绘图缓冲区高度 (行数) */
#define LVGL_DRAW_BUF_LINES     40

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
        .buffer_size = BSP_LCD_H_RES * LVGL_DRAW_BUF_LINES,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,   /* 尝试修复垂直镜像 */
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
