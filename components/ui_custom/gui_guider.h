/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"


typedef struct
{
  
	lv_obj_t *screen;
	bool screen_del;
	lv_obj_t *screen_lbl_logo;
	lv_obj_t *screen_spinner_1;
	lv_obj_t *screen_cont_modules;
	lv_obj_t *screen_cont_1;
	lv_obj_t *screen_label_RFID;
	lv_obj_t *screen_label_6;
	lv_obj_t *screen_cont_2;
	lv_obj_t *screen_label_Face;
	lv_obj_t *screen_label_7;
	lv_obj_t *screen_cont_3;
	lv_obj_t *screen_label_Finger;
	lv_obj_t *screen_label_9;
	lv_obj_t *screen_cont_4;
	lv_obj_t *screen_label_Network;
	lv_obj_t *screen_label_11;
	lv_obj_t *screen_label_fail_reason;
	lv_obj_t *screen_btn_selfcheck_return;
	lv_obj_t *screen_btn_selfcheck_return_label;
	lv_obj_t *screen_Standby;
	bool screen_Standby_del;
	lv_obj_t *screen_Standby_status_bar;
	lv_obj_t *screen_Standby_label_time;
	lv_obj_t *screen_Standby_cont_status_icons;
	lv_obj_t *screen_Standby_label_1;
	lv_obj_t *screen_Standby_label_2;
	lv_obj_t *screen_Standby_label_3;
	lv_obj_t *screen_Standby_img_1;
	lv_obj_t *screen_Standby_cont_environment;
	lv_obj_t *screen_Standby_label_temperature;
	lv_obj_t *screen_Standby_label_humidity;
	lv_obj_t *screen_Standby_cont_last_result;
	lv_obj_t *screen_Standby_label_data;
	lv_obj_t *screen_Standby_label_5;
	lv_obj_t *screen_Confirm;
	bool screen_Confirm_del;
	lv_obj_t *screen_Confirm_cont_3;
	lv_obj_t *screen_Confirm_label_studentID;
	lv_obj_t *screen_Confirm_label_Card;
	lv_obj_t *screen_Confirm_label_5;
	lv_obj_t *screen_Confirm_cont_btns;
	lv_obj_t *screen_Confirm_btn_finger_check;
	lv_obj_t *screen_Confirm_btn_finger_check_label;
	lv_obj_t *screen_Confirm_btn_face_check;
	lv_obj_t *screen_Confirm_btn_face_check_label;
	lv_obj_t *screen_Confirm_control_bar;
	lv_obj_t *screen_Confirm_btn_return;
	lv_obj_t *screen_Confirm_btn_return_label;
	lv_obj_t *screen_Confirm_status_bar;
	lv_obj_t *screen_Confirm_label_time;
	lv_obj_t *screen_Confirm_cont_2;
	lv_obj_t *screen_Confirm_label_2;
	lv_obj_t *screen_Confirm_label_1;
	lv_obj_t *screen_face;
	bool screen_face_del;
	lv_obj_t *screen_face_control_bar;
	lv_obj_t *screen_face_btn_return;
	lv_obj_t *screen_face_btn_return_label;
	lv_obj_t *screen_face_status_bar;
	lv_obj_t *screen_face_label_time;
	lv_obj_t *screen_face_cont_2;
	lv_obj_t *screen_face_label_2;
	lv_obj_t *screen_face_label_1;
	lv_obj_t *screen_face_img_1;
	lv_obj_t *screen_face_cont_3;
	lv_obj_t *screen_face_label_studentID;
	lv_obj_t *screen_face_label_5;
	lv_obj_t *screen_face_label_4;
	lv_obj_t *screen_face_label_timeout;
	lv_obj_t *screen_finger;
	bool screen_finger_del;
	lv_obj_t *screen_finger_control_bar;
	lv_obj_t *screen_finger_btn_return;
	lv_obj_t *screen_finger_btn_return_label;
	lv_obj_t *screen_finger_status_bar;
	lv_obj_t *screen_finger_label_time;
	lv_obj_t *screen_finger_cont_4;
	lv_obj_t *screen_finger_label_12;
	lv_obj_t *screen_finger_label_11;
	lv_obj_t *screen_finger_img_1;
	lv_obj_t *screen_finger_cont_3;
	lv_obj_t *screen_finger_label_studentID;
	lv_obj_t *screen_finger_label_9;
	lv_obj_t *screen_finger_label_8;
	lv_obj_t *screen_finger_label_Timeout;
	lv_obj_t *screen_success;
	bool screen_success_del;
	lv_obj_t *screen_success_control_bar;
	lv_obj_t *screen_success_btn_return;
	lv_obj_t *screen_success_btn_return_label;
	lv_obj_t *screen_success_status_bar;
	lv_obj_t *screen_success_label_time;
	lv_obj_t *screen_success_cont_5;
	lv_obj_t *screen_success_label_18;
	lv_obj_t *screen_success_label_17;
	lv_obj_t *screen_success_label_7;
	lv_obj_t *screen_success_cont_3;
	lv_obj_t *screen_success_label_check_time;
	lv_obj_t *screen_success_label_Card;
	lv_obj_t *screen_success_label_studentID;
	lv_obj_t *screen_success_label_20;
	lv_obj_t *screen_success_img_1;
	lv_obj_t *screen_unregistered;
	bool screen_unregistered_del;
	lv_obj_t *screen_unregistered_control_bar;
	lv_obj_t *screen_unregistered_btn_return;
	lv_obj_t *screen_unregistered_btn_return_label;
	lv_obj_t *screen_unregistered_status_bar;
	lv_obj_t *screen_unregistered_label_time;
	lv_obj_t *screen_unregistered_cont_6;
	lv_obj_t *screen_unregistered_label_26;
	lv_obj_t *screen_unregistered_label_25;
	lv_obj_t *screen_unregistered_label_7;
	lv_obj_t *screen_unregistered_img_1;
	lv_obj_t *screen_unregistered_label_Card;
	lv_obj_t *screen_unregistered_label_28;
	lv_obj_t *screen_admin;
	bool screen_admin_del;
	lv_obj_t *screen_admin_cont_3;
	lv_obj_t *screen_admin_label_10;
	lv_obj_t *screen_admin_label_9;
	lv_obj_t *screen_admin_label_5;
	lv_obj_t *screen_admin_cont_btns;
	lv_obj_t *screen_admin_btn_finger_register;
	lv_obj_t *screen_admin_btn_finger_register_label;
	lv_obj_t *screen_admin_btn_face_register;
	lv_obj_t *screen_admin_btn_face_register_label;
	lv_obj_t *screen_admin_control_bar;
	lv_obj_t *screen_admin_btn_return;
	lv_obj_t *screen_admin_btn_return_label;
	lv_obj_t *screen_admin_status_bar;
	lv_obj_t *screen_admin_label_time;
	lv_obj_t *screen_admin_cont_4;
	lv_obj_t *screen_admin_label_7;
	lv_obj_t *screen_admin_label_6;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_screen_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, uint32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                  uint32_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                  lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_completed_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_bottom_layer(void);

void setup_ui(lv_ui *ui);

void video_play(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_screen(lv_ui *ui);
void setup_scr_screen_Standby(lv_ui *ui);
void setup_scr_screen_Confirm(lv_ui *ui);
void setup_scr_screen_face(lv_ui *ui);
void setup_scr_screen_finger(lv_ui *ui);
void setup_scr_screen_success(lv_ui *ui);
void setup_scr_screen_unregistered(lv_ui *ui);
void setup_scr_screen_admin(lv_ui *ui);
LV_IMAGE_DECLARE(_RFID_RGB565A8_150x150);

LV_IMAGE_DECLARE(_finger_RGB565A8_200x125);

LV_IMAGE_DECLARE(_face_RGB565A8_200x125);
LV_IMAGE_DECLARE(_face_scan_RGB565A8_150x143);
LV_IMAGE_DECLARE(_finger_scan_RGB565A8_150x143);
LV_IMAGE_DECLARE(_success_RGB565A8_99x90);
LV_IMAGE_DECLARE(_warning_RGB565A8_100x100);

LV_IMAGE_DECLARE(_finger_regiseter_RGB565A8_200x125);

LV_IMAGE_DECLARE(_face_register_RGB565A8_200x125);

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_24)
LV_FONT_DECLARE(lv_font_montserratMedium_16)
LV_FONT_DECLARE(lv_font_montserratMedium_12)
LV_FONT_DECLARE(lv_font_montserratMedium_24)
LV_FONT_DECLARE(lv_font_montserratMedium_28)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_16)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_12)

LV_FONT_DECLARE(lv_font_ZiTiQuanWeiJunHeiW22_18)

#ifdef __cplusplus
}
#endif
#endif
