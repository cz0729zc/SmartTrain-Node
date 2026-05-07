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
static const char *TAG = "events_init";

#define SELFCHECK_STANDBY_DELAY_MS 5000

static void selfcheck_finish_cb(void *arg);
static void request_standby_screen_load(void);

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

static void load_standby_screen(void)
{
	if (s_ui == NULL) {
		s_standby_load_pending = false;
		return;
	}

	log_ui_mem("standby load begin");

	if (s_ui->screen_Standby_del) {
		log_ui_mem("standby setup begin");
		setup_scr_screen_Standby(s_ui);
		s_ui->screen_Standby_del = false;
		log_ui_mem("standby setup end");
	}

	log_ui_mem("standby screen_load begin");
	lv_screen_load(s_ui->screen_Standby);
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

	if (ok) {
		if (s_ui->screen_btn_selfcheck_return != NULL) {
			lv_obj_add_flag(s_ui->screen_btn_selfcheck_return, LV_OBJ_FLAG_HIDDEN);
		}
	} else {
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

void events_selfcheck_set_result(events_selfcheck_item_t item, bool ok, const char *log_text)
{
	ESP_LOGI(TAG, "selfcheck set begin item=%d ok=%d", (int)item, (int)ok);
	if (lvgl_port_lock(0)) {
		selfcheck_apply_result(item, ok, log_text);
		lvgl_port_unlock();
	}
	ESP_LOGI(TAG, "selfcheck set end item=%d", (int)item);
}

void events_selfcheck_finish(void)
{
	ESP_LOGI(TAG, "selfcheck finish begin");
	if (lvgl_port_lock(0)) {
		selfcheck_finish_cb(NULL);
		lvgl_port_unlock();
	}
	ESP_LOGI(TAG, "selfcheck finish end");
}
