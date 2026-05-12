/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "events_init.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"


#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif



static lv_ui *s_ui = NULL;
static bool s_selfcheck_failed = false;
static bool s_standby_load_pending = false;
static lv_timer_t *s_standby_delay_timer = NULL;
static char s_standby_time_text[16] = "--:--:--";
static bool s_standby_wifi_connected = false;
static bool s_standby_environment_valid = false;
static char s_standby_temperature_text[24] = "Temp: --.-C";
static char s_standby_humidity_text[24] = "Humi: --.-%";
static char s_admin_card_text[48] = {0};
static char s_admin_status_text[64] = "Admin mode";
static char s_confirm_student_id[32] = {0};
static char s_confirm_card_id[48] = {0};
static char s_unregistered_card_id[48] = {0};
static char s_unregistered_reason[48] = {0};
static events_return_target_t s_unregistered_return_target = EVENTS_RETURN_STANDBY;
static lv_timer_t *s_unregistered_return_timer = NULL;
static lv_obj_t *s_admin_card_write_btn = NULL;
static events_admin_card_write_cb_t s_admin_card_write_cb = NULL;
static void *s_admin_card_write_user_data = NULL;
static events_admin_return_cb_t s_admin_return_cb = NULL;
static void *s_admin_return_user_data = NULL;
static const char *TAG = "events_init";

#define SELFCHECK_STANDBY_DELAY_MS 5000
#define EVENTS_LVGL_LOCK_TIMEOUT_MS 1000
#define UNREGISTERED_AUTO_RETURN_MS 3000

static void selfcheck_finish_cb(void *arg);
static void request_standby_screen_load(void);
static void load_admin_screen(void);
static void apply_unregistered_status(void);

static void log_ui_mem(const char *stage)
{
	if (stage == NULL) {
		stage = "unknown";
	}

	ESP_LOGI(TAG,
		"%s core=%d stack_hwm=%u internal=%u dma=%u",
		stage,
		(int)xPortGetCoreID(),
		(unsigned)uxTaskGetStackHighWaterMark(NULL),
		(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
		(unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
}

static void cancel_standby_delay_timer(void)
{
	if (s_standby_delay_timer != NULL) {
		lv_timer_del(s_standby_delay_timer);
		s_standby_delay_timer = NULL;
	}
}

static void cancel_unregistered_return_timer(void)
{
	if (s_unregistered_return_timer != NULL) {
		lv_timer_del(s_unregistered_return_timer);
		s_unregistered_return_timer = NULL;
	}
}

static const char *unregistered_return_target_name(events_return_target_t target)
{
	switch (target) {
		case EVENTS_RETURN_ADMIN:
			return "admin";
		case EVENTS_RETURN_STANDBY:
		default:
			return "standby";
	}
}

static void apply_standby_environment(void)
{
	if (s_ui == NULL) {
		return;
	}

	if (s_ui->screen_Standby_label_temperature != NULL) {
		lv_label_set_text(s_ui->screen_Standby_label_temperature, s_standby_temperature_text);
		lv_obj_set_style_text_color(s_ui->screen_Standby_label_temperature,
			s_standby_environment_valid ? lv_color_hex(0x262626) : lv_color_hex(0x8C8C8C),
			LV_PART_MAIN | LV_STATE_DEFAULT);
	}
	if (s_ui->screen_Standby_label_humidity != NULL) {
		lv_label_set_text(s_ui->screen_Standby_label_humidity, s_standby_humidity_text);
		lv_obj_set_style_text_color(s_ui->screen_Standby_label_humidity,
			s_standby_environment_valid ? lv_color_hex(0x262626) : lv_color_hex(0x8C8C8C),
			LV_PART_MAIN | LV_STATE_DEFAULT);
	}
}

static void load_standby_screen(void)
{
	if (s_ui == NULL) {
		s_standby_load_pending = false;
		return;
	}

	cancel_unregistered_return_timer();
	log_ui_mem("standby load begin");

	if (s_ui->screen_Standby_del) {
		log_ui_mem("standby setup begin");
		setup_scr_screen_Standby(s_ui);
		s_ui->screen_Standby_del = false;
		log_ui_mem("standby setup end");
	}

	log_ui_mem("standby screen_load begin");
	lv_screen_load(s_ui->screen_Standby);
	if (s_ui->screen_Standby_label_time != NULL) {
		lv_label_set_text(s_ui->screen_Standby_label_time, s_standby_time_text);
	}
	if (s_ui->screen_Standby_label_1 != NULL) {
		lv_obj_set_style_text_color(s_ui->screen_Standby_label_1,
			s_standby_wifi_connected ? lv_color_hex(0x52C41A) : lv_color_hex(0xBFBFBF),
			LV_PART_MAIN | LV_STATE_DEFAULT);
	}
	if (s_ui->screen_Standby_label_2 != NULL) {
		lv_label_set_text(s_ui->screen_Standby_label_2,
			s_standby_wifi_connected ? "WiFi OK" : "WiFi OFF");
	}
	apply_standby_environment();
	log_ui_mem("standby screen_load end");
	s_standby_load_pending = false;
}

static void load_standby_screen_async_cb(void *arg)
{
	(void)arg;
	load_standby_screen();
}

static void request_standby_screen_load(void)
{
	if (s_standby_load_pending) {
		ESP_LOGI(TAG, "standby load already pending");
		return;
	}

	s_standby_load_pending = true;
	log_ui_mem("standby async request");
	if (lv_async_call(load_standby_screen_async_cb, NULL) != LV_RESULT_OK) {
		ESP_LOGW(TAG, "lv_async_call failed, load standby directly");
		load_standby_screen();
	}
}

static void standby_delay_timer_cb(lv_timer_t *timer)
{
	(void)timer;
	s_standby_delay_timer = NULL;

	request_standby_screen_load();
}

static void return_from_unregistered(void)
{
	cancel_unregistered_return_timer();

	if (s_unregistered_return_target == EVENTS_RETURN_ADMIN) {
		load_admin_screen();
	} else {
		request_standby_screen_load();
	}
}

static void unregistered_return_timer_cb(lv_timer_t *timer)
{
	(void)timer;
	s_unregistered_return_timer = NULL;
	ESP_LOGI(TAG, "unregistered auto return after 3s");
	return_from_unregistered();
}

static void set_module_status(lv_obj_t *label, bool ok)
{
	if (label == NULL) {
		return;
	}

	lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
	if (ok) {
		lv_label_set_text(label, LV_SYMBOL_OK " OK");
		lv_obj_set_style_text_color(label, lv_color_hex(0x52C41A), LV_PART_MAIN | LV_STATE_DEFAULT);
	} else {
		lv_label_set_text(label, LV_SYMBOL_CLOSE " Fail");
		lv_obj_set_style_text_color(label, lv_color_hex(0xFF4D4F), LV_PART_MAIN | LV_STATE_DEFAULT);
	}
}

static void update_selfcheck_log(const char *text)
{
	if (s_ui == NULL || s_ui->screen_label_fail_reason == NULL) {
		return;
	}
	lv_label_set_text(s_ui->screen_label_fail_reason, text);
}

static lv_obj_t *status_label_by_item(events_selfcheck_item_t item)
{
	if (s_ui == NULL) {
		return NULL;
	}

	switch (item) {
		case EVENTS_SC_RFID:
			return s_ui->screen_label_6;
		case EVENTS_SC_FACE:
			return s_ui->screen_label_7;
		case EVENTS_SC_FINGER:
			return s_ui->screen_label_9;
		case EVENTS_SC_NETWORK:
			return s_ui->screen_label_11;
		default:
			return NULL;
	}
}

static void selfcheck_apply_result(events_selfcheck_item_t item, bool ok, const char *log_text)
{
	if (s_ui == NULL) {
		return;
	}

	if (!ok) {
		s_selfcheck_failed = true;
		if (s_ui->screen_btn_selfcheck_return != NULL) {
			lv_obj_clear_flag(s_ui->screen_btn_selfcheck_return, LV_OBJ_FLAG_HIDDEN);
			lv_obj_move_foreground(s_ui->screen_btn_selfcheck_return);
		}
	}

	set_module_status(status_label_by_item(item), ok);
	if (log_text != NULL && log_text[0] != '\0') {
		update_selfcheck_log(log_text);
	}
}

static void selfcheck_finish_cb(void *arg)
{
	(void)arg;
	if (s_ui == NULL) {
		return;
	}

	if (!s_selfcheck_failed) {
		update_selfcheck_log("Self-check complete, entering Standby in 5s");
		cancel_standby_delay_timer();
		s_standby_delay_timer = lv_timer_create(standby_delay_timer_cb, SELFCHECK_STANDBY_DELAY_MS, NULL);
		if (s_standby_delay_timer == NULL) {
			/* 定时器创建失败时，退化为立即切屏，避免流程卡住 */
			load_standby_screen();
		}
	} else {
		cancel_standby_delay_timer();
		update_selfcheck_log("Self-check failed, press Return to Standby");
		if (s_ui->screen_btn_selfcheck_return != NULL) {
			lv_obj_clear_flag(s_ui->screen_btn_selfcheck_return, LV_OBJ_FLAG_HIDDEN);
			lv_obj_move_foreground(s_ui->screen_btn_selfcheck_return);
		}
	}
}

static void selfcheck_return_btn_cb(lv_event_t *e)
{
	(void)e;
	if (s_ui == NULL) {
		return;
	}

	request_standby_screen_load();
}

static void log_touch_point_from_event(const char *tag_prefix, lv_event_t *e)
{
	lv_indev_t *indev = NULL;
	if (e != NULL) {
		indev = lv_event_get_indev(e);
	}
	if (indev == NULL) {
		indev = lv_indev_get_act();
	}

	if (indev == NULL) {
		ESP_LOGI(TAG, "%s: indev=NULL", tag_prefix);
		return;
	}

	lv_point_t point = {0};
	lv_indev_get_point(indev, &point);
	ESP_LOGI(TAG, "%s: x=%d, y=%d", tag_prefix, (int)point.x, (int)point.y);
}

static void selfcheck_return_btn_press_dbg_cb(lv_event_t *e)
{
	if (e == NULL) {
		return;
	}

	log_touch_point_from_event("Return 按下坐标", e);

	lv_obj_t *target = lv_event_get_target(e);
	if (target != NULL) {
		lv_area_t area;
		lv_obj_get_coords(target, &area);
		ESP_LOGI(TAG, "Return 按钮区域: x1=%d y1=%d x2=%d y2=%d",
			(int)area.x1, (int)area.y1, (int)area.x2, (int)area.y2);
	}
}

static void selfcheck_return_btn_click_dbg_cb(lv_event_t *e)
{
	log_touch_point_from_event("Return 点击坐标", e);
}

static void selfcheck_screen_press_dbg_cb(lv_event_t *e)
{
	log_touch_point_from_event("Selfcheck 屏幕按下", e);
}

static void admin_card_write_btn_cb(lv_event_t *e)
{
	(void)e;
	if (s_admin_card_write_cb != NULL) {
		s_admin_card_write_cb(s_admin_card_write_user_data);
	}
}

static void admin_return_btn_cb(lv_event_t *e)
{
	(void)e;
	if (s_admin_return_cb != NULL) {
		s_admin_return_cb(s_admin_return_user_data);
	}
}

static void unregistered_return_btn_cb(lv_event_t *e)
{
	(void)e;
	ESP_LOGI(TAG, "unregistered return clicked");
	return_from_unregistered();
}

static void ensure_admin_card_write_button(void)
{
	if (s_ui == NULL || s_ui->screen_admin_control_bar == NULL) {
		return;
	}

	if (s_admin_card_write_btn != NULL) {
		return;
	}

	s_admin_card_write_btn = lv_button_create(s_ui->screen_admin_control_bar);
	lv_obj_set_pos(s_admin_card_write_btn, 350, 0);
	lv_obj_set_size(s_admin_card_write_btn, 100, 30);
	lv_obj_clear_flag(s_admin_card_write_btn, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(s_admin_card_write_btn, admin_card_write_btn_cb, LV_EVENT_CLICKED, NULL);
	lv_obj_set_style_bg_opa(s_admin_card_write_btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(s_admin_card_write_btn, lv_color_hex(0x1677FF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(s_admin_card_write_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_radius(s_admin_card_write_btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_shadow_width(s_admin_card_write_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	lv_obj_t *label = lv_label_create(s_admin_card_write_btn);
	lv_label_set_text(label, "写卡模式");
	lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
	lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(label, &lv_font_SourceHanSerifSC_Regular_12, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_center(label);
}

static void load_admin_screen(void)
{
	if (s_ui == NULL) {
		return;
	}

	cancel_unregistered_return_timer();

	if (s_ui->screen_admin_del) {
		setup_scr_screen_admin(s_ui);
		s_ui->screen_admin_del = false;
		s_admin_card_write_btn = NULL;
		if (s_ui->screen_admin_btn_return != NULL) {
			lv_obj_add_event_cb(s_ui->screen_admin_btn_return, admin_return_btn_cb, LV_EVENT_CLICKED, NULL);
		}
	}
	ensure_admin_card_write_button();
	if (s_ui->screen_admin_label_9 != NULL) {
		char buf[64];
		snprintf(buf, sizeof(buf), "瀛﹀憳:%s", s_admin_card_text);
		lv_label_set_text(s_ui->screen_admin_label_9, buf);
	}
	if (s_ui->screen_admin_label_5 != NULL) {
		lv_label_set_text(s_ui->screen_admin_label_5, s_admin_status_text);
	}
	lv_screen_load(s_ui->screen_admin);
}

void events_init(lv_ui *ui)
{
	s_ui = ui;

	if (s_ui == NULL) {
		return;
	}

	cancel_standby_delay_timer();
	update_selfcheck_log("Self-check start...");
	s_selfcheck_failed = false;

	if (s_ui->screen_btn_selfcheck_return != NULL) {
		lv_obj_add_flag(s_ui->screen_btn_selfcheck_return, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(s_ui->screen_btn_selfcheck_return, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_add_event_cb(s_ui->screen_btn_selfcheck_return, selfcheck_return_btn_cb, LV_EVENT_CLICKED, NULL);
		lv_obj_add_event_cb(s_ui->screen_btn_selfcheck_return, selfcheck_return_btn_press_dbg_cb, LV_EVENT_PRESSED, NULL);
		lv_obj_add_event_cb(s_ui->screen_btn_selfcheck_return, selfcheck_return_btn_click_dbg_cb, LV_EVENT_CLICKED, NULL);

		lv_area_t area;
		lv_obj_get_coords(s_ui->screen_btn_selfcheck_return, &area);
		ESP_LOGI(TAG, "Return 初始区域: x1=%d y1=%d x2=%d y2=%d",
			(int)area.x1, (int)area.y1, (int)area.x2, (int)area.y2);
	}

	if (s_ui->screen != NULL) {
		lv_obj_add_event_cb(s_ui->screen, selfcheck_screen_press_dbg_cb, LV_EVENT_PRESSED, NULL);
	}
}

void events_set_admin_card_write_callback(events_admin_card_write_cb_t callback, void *user_data)
{
	s_admin_card_write_cb = callback;
	s_admin_card_write_user_data = user_data;
}

void events_set_admin_return_callback(events_admin_return_cb_t callback, void *user_data)
{
	s_admin_return_cb = callback;
	s_admin_return_user_data = user_data;
}

void events_selfcheck_set_result(events_selfcheck_item_t item, bool ok, const char *log_text)
{
	ESP_LOGI(TAG, "selfcheck set begin item=%d ok=%d", (int)item, (int)ok);
	if (lvgl_port_lock(EVENTS_LVGL_LOCK_TIMEOUT_MS)) {
		selfcheck_apply_result(item, ok, log_text);
		lvgl_port_unlock();
	} else {
		ESP_LOGW(TAG, "selfcheck set skipped, LVGL lock timeout item=%d", (int)item);
	}
	ESP_LOGI(TAG, "selfcheck set end item=%d", (int)item);
}

void events_selfcheck_finish(void)
{
	ESP_LOGI(TAG, "selfcheck finish begin");
	if (lvgl_port_lock(EVENTS_LVGL_LOCK_TIMEOUT_MS)) {
		selfcheck_finish_cb(NULL);
		lvgl_port_unlock();
	} else {
		ESP_LOGW(TAG, "selfcheck finish skipped, LVGL lock timeout");
	}
	ESP_LOGI(TAG, "selfcheck finish end");
}

void events_show_standby(void)
{
	request_standby_screen_load();
}

static void apply_unregistered_status(void)
{
	if (s_ui == NULL || s_ui->screen_unregistered_del) {
		return;
	}

	if (s_ui->screen_unregistered_label_time != NULL) {
		lv_label_set_text(s_ui->screen_unregistered_label_time, s_standby_time_text);
	}
	if (s_ui->screen_unregistered_label_26 != NULL) {
		lv_obj_set_style_text_color(s_ui->screen_unregistered_label_26,
			s_standby_wifi_connected ? lv_color_hex(0x52C41A) : lv_color_hex(0xBFBFBF),
			LV_PART_MAIN | LV_STATE_DEFAULT);
	}
	if (s_ui->screen_unregistered_label_25 != NULL) {
		lv_label_set_text(s_ui->screen_unregistered_label_25,
			s_standby_wifi_connected ? "WiFi OK" : "WiFi OFF");
		lv_obj_set_style_text_color(s_ui->screen_unregistered_label_25,
			s_standby_wifi_connected ? lv_color_hex(0x1890FF) : lv_color_hex(0x8C8C8C),
			LV_PART_MAIN | LV_STATE_DEFAULT);
	}
}

static void standby_sync_ui(void *arg)
{
	(void)arg;
	if (s_ui == NULL) {
		return;
	}
	if (s_ui->screen_Standby_label_time != NULL) {
		lv_label_set_text(s_ui->screen_Standby_label_time, s_standby_time_text);
	}
	if (s_ui->screen_Standby_label_1 != NULL) {
		lv_obj_set_style_text_color(s_ui->screen_Standby_label_1,
			s_standby_wifi_connected ? lv_color_hex(0x52C41A) : lv_color_hex(0xBFBFBF),
			LV_PART_MAIN | LV_STATE_DEFAULT);
	}
	if (s_ui->screen_Standby_label_2 != NULL) {
		lv_label_set_text(s_ui->screen_Standby_label_2,
			s_standby_wifi_connected ? "WiFi OK" : "WiFi OFF");
	}
	apply_standby_environment();
	apply_unregistered_status();
}

void events_standby_update_status(const char *time_text, bool wifi_connected)
{
	if (time_text != NULL && time_text[0] != '\0') {
		strlcpy(s_standby_time_text, time_text, sizeof(s_standby_time_text));
	}
	s_standby_wifi_connected = wifi_connected;
	lv_async_call(standby_sync_ui, NULL);
}

static void standby_environment_sync_ui(void *arg)
{
	(void)arg;
	apply_standby_environment();
}

void events_standby_update_environment(float temperature, float humidity, bool valid)
{
	s_standby_environment_valid = valid;
	if (valid) {
		snprintf(s_standby_temperature_text, sizeof(s_standby_temperature_text),
			"Temp: %.1fC", temperature);
		snprintf(s_standby_humidity_text, sizeof(s_standby_humidity_text),
			"Humi: %.1f%%", humidity);
	} else {
		strlcpy(s_standby_temperature_text, "Temp: --.-C", sizeof(s_standby_temperature_text));
		strlcpy(s_standby_humidity_text, "Humi: --.-%", sizeof(s_standby_humidity_text));
	}
	lv_async_call(standby_environment_sync_ui, NULL);
}

void events_show_admin(const char *card_text)
{
	if (card_text != NULL) {
		strlcpy(s_admin_card_text, card_text, sizeof(s_admin_card_text));
	}
	if (s_ui == NULL) {
		return;
	}
	if (lvgl_port_lock(EVENTS_LVGL_LOCK_TIMEOUT_MS)) {
		load_admin_screen();
		lvgl_port_unlock();
	} else {
		ESP_LOGW(TAG, "show admin skipped, LVGL lock timeout");
	}
}

void events_show_admin_status(const char *status_text)
{
	if (status_text != NULL && status_text[0] != '\0') {
		strlcpy(s_admin_status_text, status_text, sizeof(s_admin_status_text));
	}

	if (s_ui == NULL) {
		return;
	}

	if (lvgl_port_lock(EVENTS_LVGL_LOCK_TIMEOUT_MS)) {
		load_admin_screen();
		lvgl_port_unlock();
	} else {
		ESP_LOGW(TAG, "show admin status skipped, LVGL lock timeout");
	}
}

void events_show_confirm(const char *student_id, const char *card_id)
{
	if (student_id != NULL) {
		strlcpy(s_confirm_student_id, student_id, sizeof(s_confirm_student_id));
	}
	if (card_id != NULL) {
		strlcpy(s_confirm_card_id, card_id, sizeof(s_confirm_card_id));
	}
	if (s_ui == NULL) {
		return;
	}
	if (lvgl_port_lock(EVENTS_LVGL_LOCK_TIMEOUT_MS)) {
		cancel_unregistered_return_timer();
		if (s_ui->screen_Confirm_del) {
			setup_scr_screen_Confirm(s_ui);
			s_ui->screen_Confirm_del = false;
		}
		if (s_ui->screen_Confirm_label_studentID != NULL) {
			char buf[64];
			snprintf(buf, sizeof(buf), "学号:%s", s_confirm_student_id);
			lv_label_set_text(s_ui->screen_Confirm_label_studentID, buf);
		}
		if (s_ui->screen_Confirm_label_Card != NULL) {
			char buf[64];
			snprintf(buf, sizeof(buf), "Card:%s", s_confirm_card_id);
			lv_label_set_text(s_ui->screen_Confirm_label_Card, buf);
		}
		lv_screen_load(s_ui->screen_Confirm);
		lvgl_port_unlock();
	}
}

void events_show_unregistered_ex(const char *card_id,
                                 const char *reason,
                                 events_return_target_t return_target)
{
	if (card_id != NULL) {
		strlcpy(s_unregistered_card_id, card_id, sizeof(s_unregistered_card_id));
	}
	if (reason != NULL) {
		strlcpy(s_unregistered_reason, reason, sizeof(s_unregistered_reason));
	}
	s_unregistered_return_target = return_target;
	ESP_LOGI(TAG,
		"show unregistered card=%s reason=%s return_target=%s",
		s_unregistered_card_id,
		s_unregistered_reason,
		unregistered_return_target_name(s_unregistered_return_target));
	if (s_ui == NULL) {
		return;
	}
	if (lvgl_port_lock(EVENTS_LVGL_LOCK_TIMEOUT_MS)) {
		cancel_unregistered_return_timer();
		if (s_ui->screen_unregistered_del) {
			setup_scr_screen_unregistered(s_ui);
			s_ui->screen_unregistered_del = false;
			if (s_ui->screen_unregistered_btn_return != NULL) {
				lv_obj_add_event_cb(s_ui->screen_unregistered_btn_return,
					unregistered_return_btn_cb,
					LV_EVENT_CLICKED,
					NULL);
			}
		}
		apply_unregistered_status();
		if (s_ui->screen_unregistered_label_Card != NULL) {
			char buf[64];
			snprintf(buf, sizeof(buf), "Card:%s", s_unregistered_card_id);
			lv_label_set_text(s_ui->screen_unregistered_label_Card, buf);
		}
		if (s_ui->screen_unregistered_label_28 != NULL) {
			lv_label_set_text(s_ui->screen_unregistered_label_28,
				s_unregistered_reason[0] ? s_unregistered_reason : "未知卡片");
		}
		lv_screen_load(s_ui->screen_unregistered);
		s_unregistered_return_timer = lv_timer_create(unregistered_return_timer_cb,
			UNREGISTERED_AUTO_RETURN_MS,
			NULL);
		if (s_unregistered_return_timer == NULL) {
			ESP_LOGW(TAG, "unregistered return timer create failed");
		} else {
			lv_timer_set_repeat_count(s_unregistered_return_timer, 1);
		}
		lvgl_port_unlock();
	} else {
		ESP_LOGW(TAG, "show unregistered skipped, LVGL lock timeout");
	}
}

void events_show_unregistered(const char *card_id, const char *reason)
{
	events_show_unregistered_ex(card_id, reason, EVENTS_RETURN_STANDBY);
}
