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
    lv_obj_set_size(btn, 92, 38);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_HOME);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "返回主界");
    lv_obj_center(lbl);
    return btn;
}

static void build_page_selfcheck(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_BOOT_SELFCHECK] = page;

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "启动自检");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE9F4FF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

    lv_obj_t *panel = lv_obj_create(page);
    lv_obj_set_size(panel, lv_pct(94), 145);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x112033), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x27527A), 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *list = lv_label_create(panel);
    lv_label_set_text(list,
        "RFID: 已就绪\n"
        "人脸: 已就绪\n"
        "指纹: 已就绪\n"
        "网络: 已连接");
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 12, 10);

    lv_obj_t *bar = lv_bar_create(panel);
    lv_obj_set_size(bar, lv_pct(90), 12);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 100, LV_ANIM_OFF);

    lv_obj_t *btn = lv_btn_create(page);
    lv_obj_set_size(btn, 130, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_SELFCHECK_DONE);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "回主界");
    lv_obj_center(lbl);

    create_back_btn(page);
}

static void build_page_standby(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_STANDBY] = page;

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "等待考勤");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, UI_FONT_CJK, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -36);

    lv_obj_t *sub = lv_label_create(page);
    lv_label_set_text(sub, "下一步: FACE / FINGER");
    lv_obj_set_style_text_color(sub, lv_color_hex(0xB8CCDD), 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 2);

    lv_obj_t *btn = lv_btn_create(page);
    lv_obj_set_size(btn, 186, 52);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_CARD_DETECTED);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "下一步");
    lv_obj_center(lbl);
}

static void build_page_confirm(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_USER_CONFIRM] = page;

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "身份确认");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

    lv_obj_t *card = lv_obj_create(page);
    lv_obj_set_size(card, 220, 116);
    lv_obj_align(card, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x10243A), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x285881), 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_confirm_name = lv_label_create(card);
    lv_label_set_text(s_ui.lbl_confirm_name, "姓名: 张三");
    lv_obj_align(s_ui.lbl_confirm_name, LV_ALIGN_TOP_LEFT, 10, 14);
    s_ui.lbl_confirm_sid = lv_label_create(card);
    lv_label_set_text(s_ui.lbl_confirm_sid, "学号: 2026001");
    lv_obj_align(s_ui.lbl_confirm_sid, LV_ALIGN_TOP_LEFT, 10, 40);
    s_ui.lbl_confirm_uid = lv_label_create(card);
    lv_label_set_text(s_ui.lbl_confirm_uid, "卡号: ...A7C2");
    lv_obj_align(s_ui.lbl_confirm_uid, LV_ALIGN_TOP_LEFT, 10, 66);

    lv_obj_t *btn_face = lv_btn_create(page);
    lv_obj_set_size(btn_face, 210, 56);
    lv_obj_align(btn_face, LV_ALIGN_RIGHT_MID, -12, -34);
    lv_obj_add_event_cb(btn_face, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_SELECT_FACE);
    lv_obj_t *face_txt = lv_label_create(btn_face);
    lv_label_set_text(face_txt, "FACE 考勤");
    lv_obj_center(face_txt);

    lv_obj_t *btn_finger = lv_btn_create(page);
    lv_obj_set_size(btn_finger, 210, 56);
    lv_obj_align(btn_finger, LV_ALIGN_RIGHT_MID, -12, 34);
    lv_obj_add_event_cb(btn_finger, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_SELECT_FINGER);
    lv_obj_t *finger_txt = lv_label_create(btn_finger);
    lv_label_set_text(finger_txt, "FINGER 考勤");
    lv_obj_center(finger_txt);

    create_back_btn(page);
}

static void build_page_face(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_FACE_PUNCH] = page;

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "FACE 考勤");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

    lv_obj_t *frame = lv_obj_create(page);
    lv_obj_set_size(frame, 320, 160);
    lv_obj_align(frame, LV_ALIGN_TOP_MID, 0, 32);
    lv_obj_set_style_bg_color(frame, lv_color_hex(0x0E1A28), 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(0x2E89CF), 0);
    lv_obj_set_style_radius(frame, 10, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *tip = lv_label_create(frame);
    lv_label_set_text(tip, "Please face the camera");
    lv_obj_center(tip);

    s_ui.lbl_face_status = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_face_status, "STATUS: RUN");
    lv_obj_align(s_ui.lbl_face_status, LV_ALIGN_BOTTOM_MID, 0, -18);

    lv_obj_t *ok_btn = lv_btn_create(page);
    lv_obj_set_size(ok_btn, 92, 38);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
    lv_obj_add_event_cb(ok_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_FACE_OK);
    lv_obj_t *ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "已成功");
    lv_obj_center(ok_lbl);

    create_back_btn(page);
}

static void build_page_finger(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_FINGER_PUNCH] = page;

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "FINGER 考勤");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

    lv_obj_t *icon = lv_label_create(page);
    lv_label_set_text(icon, "[ FINGER ]");
    lv_obj_set_style_text_font(icon, UI_FONT_CJK, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -26);

    s_ui.lbl_finger_status = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_finger_status, "STATUS: RUN");
    lv_obj_align(s_ui.lbl_finger_status, LV_ALIGN_CENTER, 0, 18);

    lv_obj_t *ok_btn = lv_btn_create(page);
    lv_obj_set_size(ok_btn, 92, 38);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
    lv_obj_add_event_cb(ok_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)APP_LVGL_EVT_FINGER_OK);
    lv_obj_t *ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "已成功");
    lv_obj_center(ok_lbl);

    create_back_btn(page);
}

static void build_page_result(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_RESULT] = page;

    s_ui.lbl_result_icon = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_result_icon, "[OK]");
    lv_obj_align(s_ui.lbl_result_icon, LV_ALIGN_TOP_MID, 0, 24);

    s_ui.lbl_result_title = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_result_title, "打卡成功");
    lv_obj_align(s_ui.lbl_result_title, LV_ALIGN_TOP_MID, 0, 62);

    s_ui.lbl_result_detail = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_result_detail, "张三  2026001  10:20:35\n方式: 人脸");
    lv_obj_align(s_ui.lbl_result_detail, LV_ALIGN_TOP_MID, 0, 96);

    s_ui.lbl_result_reason = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_result_reason, "");
    lv_obj_align(s_ui.lbl_result_reason, LV_ALIGN_TOP_MID, 0, 146);

    create_back_btn(page);
}

static void build_page_unregistered(void)
{
    lv_obj_t *page = create_page_container();
    s_ui.pages[APP_LVGL_PAGE_UNREGISTERED] = page;

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "未注册或未绑定");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    s_ui.lbl_unreg_reason = lv_label_create(page);
    lv_label_set_text(s_ui.lbl_unreg_reason, "检测到未注册卡片\n请联系管理员完成建档与绑定");
    lv_obj_align(s_ui.lbl_unreg_reason, LV_ALIGN_CENTER, 0, -4);

    create_back_btn(page);
}

static void build_ui_layout(void)
{
    s_ui.screen = lv_scr_act();
    lv_obj_set_style_bg_color(s_ui.screen, lv_color_hex(0x081321), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(s_ui.screen, lv_color_hex(0x0E2238), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(s_ui.screen, LV_GRAD_DIR_VER, LV_PART_MAIN);

    s_ui.top_bar = lv_obj_create(s_ui.screen);
    lv_obj_remove_style_all(s_ui.top_bar);
    lv_obj_set_size(s_ui.top_bar, lv_pct(100), UI_TOP_BAR_H);
    lv_obj_align(s_ui.top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.top_bar, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(s_ui.top_bar, lv_color_hex(0x12273C), 0);
    lv_obj_clear_flag(s_ui.top_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_clock = lv_label_create(s_ui.top_bar);
    lv_label_set_text(s_ui.lbl_clock, "08:00:00");
    lv_obj_align(s_ui.lbl_clock, LV_ALIGN_LEFT_MID, 10, 0);

    s_ui.lbl_modules = lv_label_create(s_ui.top_bar);
    lv_label_set_text(s_ui.lbl_modules, "RFID|FACE|FINGER|NET");
    lv_obj_align(s_ui.lbl_modules, LV_ALIGN_CENTER, 0, 0);

    s_ui.lbl_net = lv_label_create(s_ui.top_bar);
    lv_label_set_text(s_ui.lbl_net, "NET:在线");
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
    lv_obj_set_style_bg_opa(s_ui.bottom_bar, LV_OPA_20, 0);
    lv_obj_set_style_bg_color(s_ui.bottom_bar, lv_color_hex(0x12273C), 0);
    lv_obj_clear_flag(s_ui.bottom_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_hint = lv_label_create(s_ui.bottom_bar);
    lv_label_set_text(s_ui.lbl_hint, "提示: 点击按钮继续");
    lv_obj_align(s_ui.lbl_hint, LV_ALIGN_LEFT_MID, 10, 0);

    s_ui.lbl_countdown = lv_label_create(s_ui.bottom_bar);
    lv_label_set_text(s_ui.lbl_countdown, "");
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
            lv_label_set_text(s_ui.lbl_hint, "启动检查完成后回主界");
            break;
        case APP_LVGL_PAGE_STANDBY:
            lv_label_set_text(s_ui.lbl_hint, "请选择考勤方式");
            break;
        case APP_LVGL_PAGE_USER_CONFIRM:
            lv_label_set_text(s_ui.lbl_hint, "请选择 FACE 或 FINGER");
            break;
        case APP_LVGL_PAGE_FACE_PUNCH:
            lv_label_set_text(s_ui.lbl_hint, "请正视设备进行识别");
            break;
        case APP_LVGL_PAGE_FINGER_PUNCH:
            lv_label_set_text(s_ui.lbl_hint, "请按压指纹区域");
            break;
        case APP_LVGL_PAGE_RESULT:
            lv_label_set_text(s_ui.lbl_hint, "查看结果后可回主界");
            break;
        case APP_LVGL_PAGE_UNREGISTERED:
            lv_label_set_text(s_ui.lbl_hint, "当前卡片未注册或未绑定");
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
                lv_label_set_text(s_ui.lbl_face_status, "STATUS: RUN");
            }
            app_lvgl_show_page_locked(APP_LVGL_PAGE_FACE_PUNCH);
            break;
        case APP_LVGL_EVT_SELECT_FINGER:
            if (s_ui.lbl_finger_status != NULL) {
                lv_label_set_text(s_ui.lbl_finger_status, "STATUS: RUN");
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
    lvgl_port_lock(0);
    app_lvgl_dispatch_event_locked(event);
    lvgl_port_unlock();
}

void app_lvgl_update_user_info(const char *student_id, const char *name, const char *uid_short)
{
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
        snprintf(buf, sizeof(buf), "姓名: %s", s_ui.user_name[0] ? s_ui.user_name : "--");
        lv_label_set_text(s_ui.lbl_confirm_name, buf);
    }
    if (s_ui.lbl_confirm_sid != NULL) {
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "学号: %s", s_ui.user_sid[0] ? s_ui.user_sid : "--");
        lv_label_set_text(s_ui.lbl_confirm_sid, buf);
    }
    if (s_ui.lbl_confirm_uid != NULL) {
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "卡号: ...%s", s_ui.user_uid_short[0] ? s_ui.user_uid_short : "----");
        lv_label_set_text(s_ui.lbl_confirm_uid, buf);
    }

    lvgl_port_unlock();
}

void app_lvgl_update_result(bool success, const char *method, const char *reason, const char *time_text)
{
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
        snprintf(buf, sizeof(buf), "%s  %s  %s\n方式: %s",
                 s_ui.user_name[0] ? s_ui.user_name : "--",
                 s_ui.user_sid[0] ? s_ui.user_sid : "--",
                 safe_time,
                 safe_method);
        lv_label_set_text(s_ui.lbl_result_detail, buf);
    }
    if (s_ui.lbl_result_reason != NULL) {
        if (success || safe_reason[0] == '\0') {
            lv_label_set_text(s_ui.lbl_result_reason, "");
        } else {
            char buf[96] = {0};
            snprintf(buf, sizeof(buf), "原因: %s", safe_reason);
            lv_label_set_text(s_ui.lbl_result_reason, buf);
        }
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
