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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "app_lvgl";

#define UI_FONT_CJK (&lv_font_source_han_sans_sc_14_cjk)

/* LVGL 显示和输入设备 */
static lv_display_t *s_lvgl_disp = NULL;
static lv_indev_t *s_lvgl_touch = NULL;
static bool s_lvgl_port_ready = false;

/*
 * 绘图缓冲区高度 (行数)
 * 480x320 横屏下若使用双缓冲，DMA 内存占用会显著增大，
 * 在当前系统内存布局下可能触发 "Not enough memory for LVGL buffer"。
 */
#define LVGL_DRAW_BUF_LINES     20

/*
 * 固定横屏显示：
 * - 物理分辨率保持 BSP_LCD_H_RES/BSP_LCD_V_RES（320x480）
 * - 通过 rotation 旋转成逻辑横屏（480x320）
 */
#define APP_LVGL_LANDSCAPE_SWAP_XY   true
#define APP_LVGL_LANDSCAPE_MIRROR_X  false
#define APP_LVGL_LANDSCAPE_MIRROR_Y  true

/* 逻辑分辨率：横屏时将宽高交换，避免只渲染到左侧区域 */
#define APP_LVGL_LOGICAL_H_RES  BSP_LCD_V_RES
#define APP_LVGL_LOGICAL_V_RES  BSP_LCD_H_RES

#define UI_TOP_BAR_H            36
#define UI_BOTTOM_BAR_H         44
#define UI_FALLBACK_SECONDS     10

typedef struct {
    bool created;
    app_lvgl_page_t current_page;

    lv_obj_t *screen;
    lv_obj_t *top_bar;
    lv_obj_t *content;
    lv_obj_t *bottom_bar;

    lv_obj_t *lbl_clock;
    lv_obj_t *lbl_net;
    lv_obj_t *lbl_modules;
    lv_obj_t *lbl_hint;
    lv_obj_t *lbl_countdown;

    lv_obj_t *pages[APP_LVGL_PAGE_COUNT];

    lv_obj_t *lbl_confirm_name;
    lv_obj_t *lbl_confirm_sid;
    lv_obj_t *lbl_confirm_uid;

    lv_obj_t *lbl_face_status;
    lv_obj_t *lbl_finger_status;

    lv_obj_t *lbl_result_icon;
    lv_obj_t *lbl_result_title;
    lv_obj_t *lbl_result_detail;
    lv_obj_t *lbl_result_reason;

    lv_obj_t *lbl_unreg_reason;

    lv_timer_t *timer_clock;
    lv_timer_t *timer_fallback;
    lv_timer_t *timer_cycle;
    int fallback_left;
    bool cycle_mode;
    uint8_t cycle_idx;

    char user_name[32];
    char user_sid[24];
    char user_uid_short[16];
} app_lvgl_ui_ctx_t;

static app_lvgl_ui_ctx_t s_ui = {0};
static app_lvgl_action_cb_t s_action_cb = NULL;
static void *s_action_arg = NULL;

static void app_lvgl_show_page_locked(app_lvgl_page_t page);
static void cycle_test_timer_cb(lv_timer_t *timer);

static void apply_cjk_font_recursive(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_set_style_text_font(obj, UI_FONT_CJK, 0);

    uint32_t child_cnt = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < child_cnt; ++i) {
        lv_obj_t *child = lv_obj_get_child(obj, i);
        apply_cjk_font_recursive(child);
    }
}

static void btn_event_cb(lv_event_t *e)
{
    app_lvgl_event_t event = (app_lvgl_event_t)(intptr_t)lv_event_get_user_data(e);
    app_lvgl_dispatch_event(event);
    if (s_action_cb != NULL) {
        s_action_cb(event, s_action_arg);
    }
}

static void update_clock_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_ui.lbl_clock == NULL) {
        return;
    }

    static uint32_t fake_sec = 0;
    ++fake_sec;
    uint32_t hh = (8 + (fake_sec / 3600U)) % 24U;
    uint32_t mm = (fake_sec / 60U) % 60U;
    uint32_t ss = fake_sec % 60U;

    char buf[16] = {0};
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)hh, (unsigned)mm, (unsigned)ss);
    lv_label_set_text(s_ui.lbl_clock, buf);
}

static void update_countdown_label(void)
{
    if (s_ui.lbl_countdown == NULL) {
        return;
    }

    if (s_ui.fallback_left > 0) {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "自动返回主界 %ds", s_ui.fallback_left);
        lv_label_set_text(s_ui.lbl_countdown, buf);
    } else {
        lv_label_set_text(s_ui.lbl_countdown, "");
    }
}

static bool page_need_fallback(app_lvgl_page_t page)
{
    if (s_ui.cycle_mode) {
        return false;
    }
    return page != APP_LVGL_PAGE_STANDBY;
}

static void cycle_test_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    static const app_lvgl_page_t order[] = {
        APP_LVGL_PAGE_BOOT_SELFCHECK,
        APP_LVGL_PAGE_STANDBY,
        APP_LVGL_PAGE_USER_CONFIRM,
        APP_LVGL_PAGE_FACE_PUNCH,
        APP_LVGL_PAGE_FINGER_PUNCH,
        APP_LVGL_PAGE_RESULT,
        APP_LVGL_PAGE_UNREGISTERED,
    };

    s_ui.cycle_idx = (uint8_t)((s_ui.cycle_idx + 1U) % (sizeof(order) / sizeof(order[0])));
    app_lvgl_page_t next_page = order[s_ui.cycle_idx];

    if (next_page == APP_LVGL_PAGE_USER_CONFIRM) {
        app_lvgl_update_user_info("2026001", "张三", "A7C2");
    } else if (next_page == APP_LVGL_PAGE_RESULT) {
        bool ok = ((s_ui.cycle_idx & 0x1U) == 0U);
        app_lvgl_update_result(ok,
                               ok ? "人脸" : "指纹",
                               ok ? "" : "识别未匹配",
                               "10:20:35");
    } else if (next_page == APP_LVGL_PAGE_UNREGISTERED) {
        if (s_ui.lbl_unreg_reason != NULL) {
            if ((s_ui.cycle_idx & 0x1U) == 0U) {
                lv_label_set_text(s_ui.lbl_unreg_reason, "检测到未注册卡片\n请联系管理员完成建档");
            } else {
                lv_label_set_text(s_ui.lbl_unreg_reason, "账号已建档但未绑定生物信息\n请先完成人脸或指纹绑定");
            }
        }
    }

    app_lvgl_show_page_locked(next_page);
}

static void fallback_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!page_need_fallback(s_ui.current_page)) {
        s_ui.fallback_left = 0;
        update_countdown_label();
        return;
    }

    if (s_ui.fallback_left > 0) {
        --s_ui.fallback_left;
        update_countdown_label();
    }

    if (s_ui.fallback_left == 0) {
        app_lvgl_show_page_locked(APP_LVGL_PAGE_STANDBY);
    }
}

static lv_obj_t *create_page_container(void)
{
    lv_obj_t *page = lv_obj_create(s_ui.content);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    return page;
}

static lv_obj_t *create_back_btn(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 116, 42);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 12, -8);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xD9413A), 0);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0xB32622), 0);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x99221E), 0);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_HOME);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "CANCEL / 返回");
    lv_obj_center(lbl);
    return btn;
}

static void build_page_selfcheck(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_BOOT_SELFCHECK] = page;

    lv_obj_set_style_bg_color(page, lv_color_hex(0xEFF3F7), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);

    lv_obj_t *logo_box = lv_obj_create(page);
    lv_obj_set_size(logo_box, 220, 70);
    lv_obj_align(logo_box, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(logo_box, lv_color_hex(0xEFF3F7), 0);
    lv_obj_set_style_border_width(logo_box, 0, 0);
    lv_obj_set_style_shadow_width(logo_box, 0, 0);
    lv_obj_clear_flag(logo_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *logo = lv_label_create(logo_box);
    lv_label_set_text(logo, "LOGO\nSMART ACCESS");
    lv_obj_set_style_text_align(logo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(logo, lv_color_hex(0x21384D), 0);
    lv_obj_center(logo);

    lv_obj_t *mods = lv_label_create(page);
    lv_label_set_text(mods,
                      "[OK] RFID Module\n"
                      "[X]  Face Module   Unready\n"
                      "[OK] Fingerprint\n"
                      "[OK] Network");
    lv_obj_set_style_text_color(mods, lv_color_hex(0x1E2E40), 0);
    lv_obj_align(mods, LV_ALIGN_TOP_MID, 0, 96);

    lv_obj_t *spinner = lv_spinner_create(page);
    lv_obj_set_size(spinner, 58, 58);
    lv_obj_align(spinner, LV_ALIGN_BOTTOM_MID, 0, -48);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0xD4DFEB), LV_PART_MAIN);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x2A86DC), LV_PART_INDICATOR);

    lv_obj_t *status = lv_label_create(page);
    lv_label_set_text(status, "Face Module Unready");
    lv_obj_set_style_text_color(status, lv_color_hex(0xB3332E), 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void build_page_standby(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_STANDBY] = page;

    lv_obj_set_style_bg_color(page, lv_color_hex(0xEFF3F7), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);

    lv_obj_t *logo = lv_label_create(page);
    lv_label_set_text(logo, "LOGO\nSMART ACCESS");
    lv_obj_set_style_text_align(logo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(logo, lv_color_hex(0x253C50), 0);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "PLEASE SWIPE CARD");
    lv_obj_set_style_text_color(title, lv_color_hex(0x0C2D59), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *rfid_box = lv_obj_create(page);
    lv_obj_set_size(rfid_box, 180, 96);
    lv_obj_align(rfid_box, LV_ALIGN_TOP_MID, 0, 114);
    lv_obj_set_style_bg_color(rfid_box, lv_color_hex(0xF5FAFF), 0);
    lv_obj_set_style_border_color(rfid_box, lv_color_hex(0x6A8098), 0);
    lv_obj_set_style_radius(rfid_box, 18, 0);
    lv_obj_clear_flag(rfid_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *rfid_txt = lv_label_create(rfid_box);
    lv_label_set_text(rfid_txt, "RFID");
    lv_obj_set_style_text_color(rfid_txt, lv_color_hex(0x33495E), 0);
    lv_obj_set_style_text_font(rfid_txt, &lv_font_montserrat_24, 0);
    lv_obj_center(rfid_txt);

    lv_obj_t *last = lv_obj_create(page);
    lv_obj_set_size(last, lv_pct(100), 24);
    lv_obj_align(last, LV_ALIGN_BOTTOM_MID, 0, -26);
    lv_obj_set_style_bg_color(last, lv_color_hex(0x39B467), 0);
    lv_obj_set_style_border_width(last, 0, 0);
    lv_obj_set_style_radius(last, 0, 0);
    lv_obj_clear_flag(last, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *last_txt = lv_label_create(last);
    lv_label_set_text(last_txt, "Last Check: 14:28 - Zhang San (Success)");
    lv_obj_set_style_text_color(last_txt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(last_txt);

    lv_obj_t *tip = lv_label_create(page);
    lv_label_set_text(tip, "Swipe after card, choose check-in type");
    lv_obj_set_style_text_color(tip, lv_color_hex(0x1E2C3A), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -4);

    lv_obj_t *tap_area = lv_btn_create(page);
    lv_obj_remove_style_all(tap_area);
    lv_obj_set_size(tap_area, lv_pct(100), lv_pct(100));
    lv_obj_align(tap_area, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(tap_area, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_CARD_DETECTED);
}

static void build_page_confirm(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_USER_CONFIRM] = page;

    lv_obj_set_style_bg_color(page, lv_color_hex(0xEFF3F7), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);

    lv_obj_t *logo = lv_label_create(page);
    lv_label_set_text(logo, "LOGO\nSMART ACCESS");
    lv_obj_set_style_text_align(logo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(logo, lv_color_hex(0x253C50), 0);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *card = lv_obj_create(page);
    lv_obj_set_size(card, 168, 96);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, 12, 8);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xEFF3F7), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xB8C5D1), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_confirm_name = lv_label_create(card);
    lv_label_set_text(s_ui.lbl_confirm_name, "Name: Zhang San (张三)");
    lv_obj_align(s_ui.lbl_confirm_name, LV_ALIGN_TOP_LEFT, 8, 8);
    s_ui.lbl_confirm_sid = lv_label_create(card);
    lv_label_set_text(s_ui.lbl_confirm_sid, "ID: 20230101");
    lv_obj_align(s_ui.lbl_confirm_sid, LV_ALIGN_TOP_LEFT, 8, 32);
    s_ui.lbl_confirm_uid = lv_label_create(card);
    lv_label_set_text(s_ui.lbl_confirm_uid, "Card: *CCDD");
    lv_obj_align(s_ui.lbl_confirm_uid, LV_ALIGN_TOP_LEFT, 8, 56);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "PLEASE CHOOSE CHECK-IN");
    lv_obj_set_style_text_color(title, lv_color_hex(0x0C2D59), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 86);

    lv_obj_t *btn_face = lv_btn_create(page);
    lv_obj_set_size(btn_face, 214, 86);
    lv_obj_align(btn_face, LV_ALIGN_TOP_LEFT, 20, 148);
    lv_obj_set_style_bg_color(btn_face, lv_color_hex(0x4D95DA), 0);
    lv_obj_set_style_radius(btn_face, 12, 0);
    lv_obj_add_event_cb(btn_face, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_SELECT_FACE);
    lv_obj_t *face_txt = lv_label_create(btn_face);
    lv_label_set_text(face_txt, "FACE打卡");
    lv_obj_set_style_text_color(face_txt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(face_txt, &lv_font_montserrat_24, 0);
    lv_obj_center(face_txt);

    lv_obj_t *btn_finger = lv_btn_create(page);
    lv_obj_set_size(btn_finger, 214, 86);
    lv_obj_align(btn_finger, LV_ALIGN_TOP_RIGHT, -20, 148);
    lv_obj_set_style_bg_color(btn_finger, lv_color_hex(0x4D95DA), 0);
    lv_obj_set_style_radius(btn_finger, 12, 0);
    lv_obj_add_event_cb(btn_finger, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_SELECT_FINGER);
    lv_obj_t *finger_txt = lv_label_create(btn_finger);
    lv_label_set_text(finger_txt, "FINGER打卡");
    lv_obj_set_style_text_color(finger_txt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(finger_txt, &lv_font_montserrat_24, 0);
    lv_obj_center(finger_txt);

    lv_obj_t *timeout = lv_label_create(page);
    lv_label_set_text(timeout, "Timeout: 10s");
    lv_obj_set_style_text_color(timeout, lv_color_hex(0x1B2E41), 0);
    lv_obj_align(timeout, LV_ALIGN_BOTTOM_RIGHT, -12, -6);

    create_back_btn(page);
}

static void build_page_face(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_FACE_PUNCH] = page;

    lv_obj_set_style_bg_color(page, lv_color_hex(0xEFF3F7), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);

    lv_obj_t *ring = lv_arc_create(page);
    lv_obj_set_size(ring, 132, 132);
    lv_obj_align(ring, LV_ALIGN_TOP_MID, 0, 8);
    lv_arc_set_range(ring, 0, 100);
    lv_arc_set_value(ring, 24);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(ring, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring, lv_color_hex(0xC5D5E7), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, lv_color_hex(0x2E89DF), LV_PART_INDICATOR);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "正视设备");
    lv_obj_set_style_text_color(title, lv_color_hex(0x0C2D59), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 146);

    lv_obj_t *detection = lv_label_create(page);
    lv_label_set_text(detection, "Detection: 正在检测");
    lv_obj_set_style_text_color(detection, lv_color_hex(0x1C2E42), 0);
    lv_obj_set_style_text_font(detection, &lv_font_montserrat_24, 0);
    lv_obj_align(detection, LV_ALIGN_TOP_MID, 0, 196);

    s_ui.lbl_face_status = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_face_status, "Recognition: 等待结果");
    lv_obj_set_style_text_color(s_ui.lbl_face_status, lv_color_hex(0x1C2E42), 0);
    lv_obj_set_style_text_font(s_ui.lbl_face_status, &lv_font_montserrat_24, 0);
    lv_obj_align(s_ui.lbl_face_status, LV_ALIGN_TOP_MID, 0, 226);

    lv_obj_t *info = lv_label_create(page);
    lv_label_set_text(info, "Name: Zhang San (张三)\nCard: *CCDD");
    lv_obj_set_style_text_color(info, lv_color_hex(0x1C2E42), 0);
    lv_obj_align(info, LV_ALIGN_BOTTOM_LEFT, 18, -4);

    lv_obj_t *ok_btn = lv_btn_create(page);
    lv_obj_set_size(ok_btn, 118, 42);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
    lv_obj_set_style_bg_color(ok_btn, lv_color_hex(0x3CB971), 0);
    lv_obj_add_event_cb(ok_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_FACE_OK);
    lv_obj_t *ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "模拟成功");
    lv_obj_center(ok_lbl);

    create_back_btn(page);
}

static void build_page_finger(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_FINGER_PUNCH] = page;

    lv_obj_set_style_bg_color(page, lv_color_hex(0xEFF3F7), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);

    lv_obj_t *brief = lv_obj_create(page);
    lv_obj_set_size(brief, 220, 46);
    lv_obj_align(brief, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_bg_color(brief, lv_color_hex(0xEFF3F7), 0);
    lv_obj_set_style_border_color(brief, lv_color_hex(0xB8C5D1), 0);
    lv_obj_set_style_radius(brief, 12, 0);
    lv_obj_clear_flag(brief, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *brief_txt = lv_label_create(brief);
    lv_label_set_text(brief_txt, "Zhang San (张三)\nCard: *CCDD");
    lv_obj_center(brief_txt);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "请按压指纹");
    lv_obj_set_style_text_color(title, lv_color_hex(0x0C2D59), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 56);

    lv_obj_t *scan = lv_label_create(page);
    lv_label_set_text(scan, "Scanning:");
    lv_obj_align(scan, LV_ALIGN_TOP_LEFT, 36, 120);

    lv_obj_t *fingerprint = lv_label_create(page);
    lv_label_set_text(fingerprint, "[ FINGER ]");
    lv_obj_set_style_text_color(fingerprint, lv_color_hex(0x8C959F), 0);
    lv_obj_set_style_text_font(fingerprint, &lv_font_montserrat_24, 0);
    lv_obj_align(fingerprint, LV_ALIGN_CENTER, 16, 8);

    lv_obj_t *scan_state = lv_label_create(page);
    lv_label_set_text(scan_state, "采集中");
    lv_obj_set_style_text_color(scan_state, lv_color_hex(0x2B7ECC), 0);
    lv_obj_align(scan_state, LV_ALIGN_TOP_LEFT, 36, 152);

    lv_obj_t *match = lv_label_create(page);
    lv_label_set_text(match, "Matching:");
    lv_obj_align(match, LV_ALIGN_TOP_LEFT, 36, 188);

    s_ui.lbl_finger_status = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_finger_status, "等待匹配");
    lv_obj_set_style_text_color(s_ui.lbl_finger_status, lv_color_hex(0x1C2E42), 0);
    lv_obj_align(s_ui.lbl_finger_status, LV_ALIGN_TOP_LEFT, 36, 220);

    lv_obj_t *ok_btn = lv_btn_create(page);
    lv_obj_set_size(ok_btn, 118, 42);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
    lv_obj_set_style_bg_color(ok_btn, lv_color_hex(0x3CB971), 0);
    lv_obj_add_event_cb(ok_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_FINGER_OK);
    lv_obj_t *ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "模拟成功");
    lv_obj_center(ok_lbl);

    create_back_btn(page);
}

static void build_page_result(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_RESULT] = page;

    lv_obj_set_style_bg_color(page, lv_color_hex(0xE8F6EC), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);

    s_ui.lbl_result_icon = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_result_icon, "[OK]");
    lv_obj_set_style_text_color(s_ui.lbl_result_icon, lv_color_hex(0x2BAA64), 0);
    lv_obj_set_style_text_font(s_ui.lbl_result_icon, &lv_font_montserrat_24, 0);
    lv_obj_align(s_ui.lbl_result_icon, LV_ALIGN_TOP_MID, 0, 14);

    s_ui.lbl_result_title = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_result_title, "打卡成功");
    lv_obj_set_style_text_color(s_ui.lbl_result_title, lv_color_hex(0x2BAA64), 0);
    lv_obj_set_style_text_font(s_ui.lbl_result_title, &lv_font_montserrat_24, 0);
    lv_obj_align(s_ui.lbl_result_title, LV_ALIGN_TOP_MID, 0, 58);

    lv_obj_t *panel = lv_obj_create(page);
    lv_obj_set_size(panel, 280, 110);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 118);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xF6FBF7), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_result_detail = lv_label_create(panel);
    lv_label_set_text(s_ui.lbl_result_detail, "张三  2026001  10:20:35\n方式: 人脸");
    lv_obj_set_style_text_align(s_ui.lbl_result_detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_ui.lbl_result_detail, LV_ALIGN_CENTER, 0, -14);

    s_ui.lbl_result_reason = lv_label_create(panel);
    lv_label_set_text(s_ui.lbl_result_reason, "");
    lv_obj_set_style_text_align(s_ui.lbl_result_reason, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_ui.lbl_result_reason, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_obj_t *countdown = lv_label_create(page);
    lv_label_set_text(countdown, "3秒后自动返回主页");
    lv_obj_set_style_text_color(countdown, lv_color_hex(0x1F2E3F), 0);
    lv_obj_align(countdown, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_obj_t *ok_btn = lv_btn_create(page);
    lv_obj_set_size(ok_btn, 180, 42);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(ok_btn, lv_color_hex(0x3CB971), 0);
    lv_obj_add_event_cb(ok_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_HOME);
    lv_obj_t *ok_txt = lv_label_create(ok_btn);
    lv_label_set_text(ok_txt, "OK / 确认");
    lv_obj_center(ok_txt);
}

static void build_page_unregistered(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_UNREGISTERED] = page;

    lv_obj_set_style_bg_color(page, lv_color_hex(0xFDEEDB), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);

    lv_obj_t *warn = lv_label_create(page);
    lv_label_set_text(warn, "!");
    lv_obj_set_style_text_color(warn, lv_color_hex(0xF29922), 0);
    lv_obj_set_style_text_font(warn, &lv_font_montserrat_24, 0);
    lv_obj_align(warn, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t *uid = lv_label_create(page);
    lv_label_set_text(uid, "CARD UID: AABBCCDD");
    lv_obj_set_style_text_color(uid, lv_color_hex(0x21262D), 0);
    lv_obj_align(uid, LV_ALIGN_TOP_MID, 0, 66);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "未注册或未绑定");
    lv_obj_set_style_text_color(title, lv_color_hex(0x0C2D59), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 96);

    lv_obj_t *panel = lv_obj_create(page);
    lv_obj_set_size(panel, 336, 82);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 152);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFF8EE), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_unreg_reason = lv_label_create(panel);
    lv_label_set_text(s_ui.lbl_unreg_reason, "Error: Card not in database.\nAction: CONTACT ADMIN.");
    lv_obj_set_style_text_color(s_ui.lbl_unreg_reason, lv_color_hex(0x21262D), 0);
    lv_obj_align(s_ui.lbl_unreg_reason, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *admin_btn = lv_btn_create(page);
    lv_obj_set_size(admin_btn, 256, 42);
    lv_obj_align(admin_btn, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
    lv_obj_set_style_bg_color(admin_btn, lv_color_hex(0xF39828), 0);
    lv_obj_set_style_radius(admin_btn, 10, 0);
    lv_obj_add_event_cb(admin_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_HOME);
    lv_obj_t *admin_lbl = lv_label_create(admin_btn);
    lv_label_set_text(admin_lbl, "CONTACT ADMIN / 管理员入口");
    lv_obj_center(admin_lbl);

    create_back_btn(page);
}

static void build_ui_layout(void)
{
    s_ui.screen = lv_scr_act();
    lv_obj_set_style_bg_color(s_ui.screen, lv_color_hex(0xEFF3F7), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(s_ui.screen, lv_color_hex(0xE8EDF4), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(s_ui.screen, LV_GRAD_DIR_VER, LV_PART_MAIN);

    s_ui.top_bar = lv_obj_create(s_ui.screen);
    lv_obj_remove_style_all(s_ui.top_bar);
    lv_obj_set_size(s_ui.top_bar, lv_pct(100), UI_TOP_BAR_H);
    lv_obj_align(s_ui.top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_ui.top_bar, lv_color_hex(0xF7FAFD), 0);
    lv_obj_clear_flag(s_ui.top_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_clock = lv_label_create(s_ui.top_bar);
    lv_label_set_text(s_ui.lbl_clock, "14:30");
    lv_obj_set_style_text_color(s_ui.lbl_clock, lv_color_hex(0x14181C), 0);
    lv_obj_set_style_text_font(s_ui.lbl_clock, &lv_font_montserrat_24, 0);
    lv_obj_align(s_ui.lbl_clock, LV_ALIGN_LEFT_MID, 10, 0);

    s_ui.lbl_modules = lv_label_create(s_ui.top_bar);
    lv_label_set_text(s_ui.lbl_modules, "");
    lv_obj_align(s_ui.lbl_modules, LV_ALIGN_CENTER, 0, 0);

    s_ui.lbl_net = lv_label_create(s_ui.top_bar);
    lv_label_set_text(s_ui.lbl_net, "DEVICE OK");
    lv_obj_set_style_text_color(s_ui.lbl_net, lv_color_hex(0x14181C), 0);
    lv_obj_set_style_text_font(s_ui.lbl_net, &lv_font_montserrat_24, 0);
    lv_obj_align(s_ui.lbl_net, LV_ALIGN_RIGHT_MID, -10, 0);

    s_ui.content = lv_obj_create(s_ui.screen);
    lv_obj_remove_style_all(s_ui.content);
    lv_obj_set_size(s_ui.content, lv_pct(100), APP_LVGL_LOGICAL_V_RES - UI_TOP_BAR_H - UI_BOTTOM_BAR_H);
    lv_obj_align(s_ui.content, LV_ALIGN_TOP_MID, 0, UI_TOP_BAR_H);
    lv_obj_set_style_bg_opa(s_ui.content, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_ui.content, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.bottom_bar = lv_obj_create(s_ui.screen);
    lv_obj_remove_style_all(s_ui.bottom_bar);
    lv_obj_set_size(s_ui.bottom_bar, lv_pct(100), UI_BOTTOM_BAR_H);
    lv_obj_align(s_ui.bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.bottom_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_ui.bottom_bar, lv_color_hex(0xE9EEF4), 0);
    lv_obj_clear_flag(s_ui.bottom_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_hint = lv_label_create(s_ui.bottom_bar);
    lv_label_set_text(s_ui.lbl_hint, "Swipe after card, choose check-in type");
    lv_obj_set_style_text_color(s_ui.lbl_hint, lv_color_hex(0x1E2C3A), 0);
    lv_obj_align(s_ui.lbl_hint, LV_ALIGN_LEFT_MID, 10, 0);

    s_ui.lbl_countdown = lv_label_create(s_ui.bottom_bar);
    lv_label_set_text(s_ui.lbl_countdown, "");
    lv_obj_set_style_text_color(s_ui.lbl_countdown, lv_color_hex(0x1E2C3A), 0);
    lv_obj_align(s_ui.lbl_countdown, LV_ALIGN_RIGHT_MID, -10, 0);

    build_page_selfcheck();
    build_page_standby();
    build_page_confirm();
    build_page_face();
    build_page_finger();
    build_page_result();
    build_page_unregistered();

    apply_cjk_font_recursive(s_ui.screen);
}

static void set_bottom_hint_for_page(app_lvgl_page_t page)
{
    if (s_ui.lbl_hint == NULL) {
        return;
    }

    switch (page) {
        case APP_LVGL_PAGE_BOOT_SELFCHECK:
            lv_label_set_text(s_ui.lbl_hint, "Self-check in progress");
            break;
        case APP_LVGL_PAGE_STANDBY:
            lv_label_set_text(s_ui.lbl_hint, "Swipe after card, choose check-in type");
            break;
        case APP_LVGL_PAGE_USER_CONFIRM:
            lv_label_set_text(s_ui.lbl_hint, "Please choose FACE or FINGER");
            break;
        case APP_LVGL_PAGE_FACE_PUNCH:
            lv_label_set_text(s_ui.lbl_hint, "Please face device");
            break;
        case APP_LVGL_PAGE_FINGER_PUNCH:
            lv_label_set_text(s_ui.lbl_hint, "Please press fingerprint");
            break;
        case APP_LVGL_PAGE_RESULT:
            lv_label_set_text(s_ui.lbl_hint, "Result will auto return");
            break;
        case APP_LVGL_PAGE_UNREGISTERED:
            lv_label_set_text(s_ui.lbl_hint, "Card is not registered/bound");
            break;
        default:
            break;
    }
}

static void app_lvgl_show_page_locked(app_lvgl_page_t page)
{
    if (!s_ui.created || page >= APP_LVGL_PAGE_COUNT) {
        return;
    }

    for (int i = 0; i < APP_LVGL_PAGE_COUNT; ++i) {
        if (s_ui.pages[i] == NULL) {
            continue;
        }
        if (i == (int)page) {
            lv_obj_clear_flag(s_ui.pages[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ui.pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    s_ui.current_page = page;
    set_bottom_hint_for_page(page);

    if (page_need_fallback(page)) {
        s_ui.fallback_left = UI_FALLBACK_SECONDS;
    } else {
        s_ui.fallback_left = 0;
    }
    update_countdown_label();
}

static void app_lvgl_dispatch_event_locked(app_lvgl_event_t event)
{
    switch (event) {
        case APP_LVGL_EVT_SELFCHECK_DONE:
            app_lvgl_show_page_locked(APP_LVGL_PAGE_STANDBY);
            break;
        case APP_LVGL_EVT_CARD_DETECTED:
            app_lvgl_show_page_locked(APP_LVGL_PAGE_USER_CONFIRM);
            break;
        case APP_LVGL_EVT_SELECT_FACE:
            if (s_ui.lbl_face_status != NULL) {
                lv_label_set_text(s_ui.lbl_face_status, "Recognition: 等待结果");
            }
            app_lvgl_show_page_locked(APP_LVGL_PAGE_FACE_PUNCH);
            break;
        case APP_LVGL_EVT_SELECT_FINGER:
            if (s_ui.lbl_finger_status != NULL) {
                lv_label_set_text(s_ui.lbl_finger_status, "等待匹配");
            }
            app_lvgl_show_page_locked(APP_LVGL_PAGE_FINGER_PUNCH);
            break;
        case APP_LVGL_EVT_FACE_OK:
            app_lvgl_show_page_locked(APP_LVGL_PAGE_RESULT);
            break;
        case APP_LVGL_EVT_FINGER_OK:
            app_lvgl_show_page_locked(APP_LVGL_PAGE_RESULT);
            break;
        case APP_LVGL_EVT_FACE_FAIL:
            app_lvgl_show_page_locked(APP_LVGL_PAGE_RESULT);
            break;
        case APP_LVGL_EVT_FINGER_FAIL:
            app_lvgl_show_page_locked(APP_LVGL_PAGE_RESULT);
            break;
        case APP_LVGL_EVT_CARD_UNREGISTERED:
            if (s_ui.lbl_unreg_reason != NULL) {
                lv_label_set_text(s_ui.lbl_unreg_reason, "检测到未注册卡片\n请联系管理员完成建档");
            }
            app_lvgl_show_page_locked(APP_LVGL_PAGE_UNREGISTERED);
            break;
        case APP_LVGL_EVT_CARD_UNBOUND:
            if (s_ui.lbl_unreg_reason != NULL) {
                lv_label_set_text(s_ui.lbl_unreg_reason, "账号已建档但未绑定生物信息\n请先完成人脸或指纹绑定");
            }
            app_lvgl_show_page_locked(APP_LVGL_PAGE_UNREGISTERED);
            break;
        case APP_LVGL_EVT_BACK:
            app_lvgl_show_page_locked(APP_LVGL_PAGE_USER_CONFIRM);
            break;
        case APP_LVGL_EVT_CANCEL:
        case APP_LVGL_EVT_HOME:
        case APP_LVGL_EVT_TIMEOUT:
            app_lvgl_show_page_locked(APP_LVGL_PAGE_STANDBY);
            break;
        default:
            break;
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
    s_lvgl_port_ready = true;

    /* 4. 添加 LCD 显示 */
    ESP_LOGI(TAG, "添加 LCD 显示 (逻辑分辨率 %dx%d)", APP_LVGL_LOGICAL_H_RES, APP_LVGL_LOGICAL_V_RES);
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = bsp_lcd_get_io_handle(),
        .panel_handle = bsp_lcd_get_panel_handle(),
        .buffer_size = APP_LVGL_LOGICAL_H_RES * LVGL_DRAW_BUF_LINES,
        .double_buffer = false,
        .hres = APP_LVGL_LOGICAL_H_RES,
        .vres = APP_LVGL_LOGICAL_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = APP_LVGL_LANDSCAPE_SWAP_XY,
            .mirror_x = APP_LVGL_LANDSCAPE_MIRROR_X,
            .mirror_y = APP_LVGL_LANDSCAPE_MIRROR_Y,
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
    ESP_LOGI(TAG, "加载业务 UI 状态机页面");

    if (!s_lvgl_port_ready) {
        ESP_LOGE(TAG, "LVGL port 未初始化，忽略 app_lvgl_demo 调用");
        return;
    }

    lvgl_port_lock(0);

    if (!s_ui.created) {
        build_ui_layout();
        s_ui.timer_clock = lv_timer_create(update_clock_cb, 1000, NULL);
        s_ui.timer_fallback = lv_timer_create(fallback_timer_cb, 1000, NULL);
        s_ui.created = true;
    }

    app_lvgl_update_user_info("2026001", "张三", "A7C2");
    app_lvgl_show_page_locked(APP_LVGL_PAGE_BOOT_SELFCHECK);

    lvgl_port_unlock();
}

void app_lvgl_dispatch_event(app_lvgl_event_t event)
{
    if (!s_lvgl_port_ready) {
        ESP_LOGW(TAG, "LVGL port 未初始化，忽略事件: %d", (int)event);
        return;
    }

    lvgl_port_lock(0);
    app_lvgl_dispatch_event_locked(event);
    lvgl_port_unlock();
}

void app_lvgl_update_user_info(const char *student_id, const char *name, const char *uid_short)
{
    if (!s_lvgl_port_ready) {
        ESP_LOGW(TAG, "LVGL port 未初始化，忽略用户信息更新");
        return;
    }

    lvgl_port_lock(0);

    if (student_id != NULL) {
        strlcpy(s_ui.user_sid, student_id, sizeof(s_ui.user_sid));
    }
    if (name != NULL) {
        strlcpy(s_ui.user_name, name, sizeof(s_ui.user_name));
    }
    if (uid_short != NULL) {
        strlcpy(s_ui.user_uid_short, uid_short, sizeof(s_ui.user_uid_short));
    }

    if (s_ui.lbl_confirm_name != NULL) {
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "Name: %s", s_ui.user_name[0] ? s_ui.user_name : "--");
        lv_label_set_text(s_ui.lbl_confirm_name, buf);
    }
    if (s_ui.lbl_confirm_sid != NULL) {
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "ID: %s", s_ui.user_sid[0] ? s_ui.user_sid : "--");
        lv_label_set_text(s_ui.lbl_confirm_sid, buf);
    }
    if (s_ui.lbl_confirm_uid != NULL) {
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "Card: *%s", s_ui.user_uid_short[0] ? s_ui.user_uid_short : "----");
        lv_label_set_text(s_ui.lbl_confirm_uid, buf);
    }

    lvgl_port_unlock();
}

void app_lvgl_update_result(bool success, const char *method, const char *reason, const char *time_text)
{
    if (!s_lvgl_port_ready) {
        ESP_LOGW(TAG, "LVGL port 未初始化，忽略结果页更新");
        return;
    }

    lvgl_port_lock(0);

    const char *safe_method = method != NULL ? method : "--";
    const char *safe_reason = reason != NULL ? reason : "";
    const char *safe_time = time_text != NULL ? time_text : "--:--:--";

    if (s_ui.lbl_result_icon != NULL) {
        lv_label_set_text(s_ui.lbl_result_icon, success ? "[OK]" : "[X]");
        lv_obj_set_style_text_color(s_ui.lbl_result_icon,
                                    success ? lv_color_hex(0x6EE27A) : lv_color_hex(0xFF6B6B),
                                    0);
    }
    if (s_ui.lbl_result_title != NULL) {
        lv_label_set_text(s_ui.lbl_result_title, success ? "打卡成功" : "打卡失败");
        lv_obj_set_style_text_color(s_ui.lbl_result_title,
                                    success ? lv_color_hex(0x6EE27A) : lv_color_hex(0xFF6B6B),
                                    0);
    }
    if (s_ui.lbl_result_detail != NULL) {
        char buf[128] = {0};
        snprintf(buf, sizeof(buf), "Name: %s\nID: %s\nCard: *%s\nTime: %s\nMethod: %s",
                 s_ui.user_name[0] ? s_ui.user_name : "--",
                 s_ui.user_sid[0] ? s_ui.user_sid : "--",
                 s_ui.user_uid_short[0] ? s_ui.user_uid_short : "----",
                 safe_time,
                 safe_method);
        lv_label_set_text(s_ui.lbl_result_detail, buf);
    }
    if (s_ui.lbl_result_reason != NULL) {
        if (success || safe_reason[0] == '\0') {
            lv_label_set_text(s_ui.lbl_result_reason, "");
        } else {
            char buf[96] = {0};
            snprintf(buf, sizeof(buf), "Reason: %s", safe_reason);
            lv_label_set_text(s_ui.lbl_result_reason, buf);
        }
    }

    if (s_ui.pages[APP_LVGL_PAGE_RESULT] != NULL) {
        lv_obj_set_style_bg_color(s_ui.pages[APP_LVGL_PAGE_RESULT],
                                  success ? lv_color_hex(0xE8F6EC) : lv_color_hex(0xFDEBED),
                                  0);
    }

    lvgl_port_unlock();
}

void app_lvgl_set_action_callback(app_lvgl_action_cb_t callback, void *arg)
{
    s_action_cb = callback;
    s_action_arg = arg;
}

void app_lvgl_start_cycle_test(uint32_t interval_ms)
{
    if (interval_ms < 1000U) {
        interval_ms = 1000U;
    }

    if (!s_lvgl_port_ready) {
        ESP_LOGE(TAG, "LVGL port 未初始化，无法启动轮播测试");
        return;
    }

    lvgl_port_lock(0);

    if (!s_ui.created) {
        lvgl_port_unlock();
        return;
    }

    s_ui.cycle_mode = true;
    s_ui.cycle_idx = 0;
    app_lvgl_show_page_locked(APP_LVGL_PAGE_BOOT_SELFCHECK);

    if (s_ui.timer_cycle != NULL) {
        lv_timer_del(s_ui.timer_cycle);
        s_ui.timer_cycle = NULL;
    }

    s_ui.timer_cycle = lv_timer_create(cycle_test_timer_cb, interval_ms, NULL);
    ESP_LOGI(TAG, "已启用 7 页面轮播测试，间隔=%lu ms", (unsigned long)interval_ms);

    lvgl_port_unlock();
}
