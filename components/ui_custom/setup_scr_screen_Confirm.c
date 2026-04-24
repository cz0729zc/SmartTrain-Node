/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"



void setup_scr_screen_Confirm(lv_ui *ui)
{
    //Write codes screen_Confirm
    ui->screen_Confirm = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_Confirm, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_Confirm, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Confirm, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Confirm, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Confirm, lv_color_hex(0xF0F2F5), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Confirm, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_cont_3
    ui->screen_Confirm_cont_3 = lv_obj_create(ui->screen_Confirm);
    lv_obj_set_pos(ui->screen_Confirm_cont_3, 160, 31);
    lv_obj_set_size(ui->screen_Confirm_cont_3, 160, 70);
    lv_obj_set_scrollbar_mode(ui->screen_Confirm_cont_3, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Confirm_cont_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_cont_3, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_Confirm_cont_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_Confirm_cont_3, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_Confirm_cont_3, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_cont_3, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_cont_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Confirm_cont_3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Confirm_cont_3, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_label_studentID
    ui->screen_Confirm_label_studentID = lv_label_create(ui->screen_Confirm_cont_3);
    lv_obj_set_pos(ui->screen_Confirm_label_studentID, 0, 0);
    lv_obj_set_size(ui->screen_Confirm_label_studentID, 160, 35);
    lv_label_set_text(ui->screen_Confirm_label_studentID, "学号:202210120324");
    lv_label_set_long_mode(ui->screen_Confirm_label_studentID, LV_LABEL_LONG_WRAP);

    //Write style for screen_Confirm_label_studentID, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_label_studentID, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_label_studentID, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Confirm_label_studentID, lv_color_hex(0x1F1F1F), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Confirm_label_studentID, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Confirm_label_studentID, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Confirm_label_studentID, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Confirm_label_studentID, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Confirm_label_studentID, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_label_studentID, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_label_studentID, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_label_studentID, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_label_studentID, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_label_studentID, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_label_studentID, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_label_Card
    ui->screen_Confirm_label_Card = lv_label_create(ui->screen_Confirm_cont_3);
    lv_obj_set_pos(ui->screen_Confirm_label_Card, 0, 32);
    lv_obj_set_size(ui->screen_Confirm_label_Card, 160, 34);
    lv_label_set_text(ui->screen_Confirm_label_Card, "Card:AABBCCDD");
    lv_label_set_long_mode(ui->screen_Confirm_label_Card, LV_LABEL_LONG_WRAP);

    //Write style for screen_Confirm_label_Card, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Confirm_label_Card, lv_color_hex(0x1F1F1F), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Confirm_label_Card, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Confirm_label_Card, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Confirm_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Confirm_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Confirm_label_Card, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_label_Card, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_label_5
    ui->screen_Confirm_label_5 = lv_label_create(ui->screen_Confirm);
    lv_obj_set_pos(ui->screen_Confirm_label_5, 0, 104);
    lv_obj_set_size(ui->screen_Confirm_label_5, 480, 30);
    lv_label_set_text(ui->screen_Confirm_label_5, "PLEASE CHOOSE CHECK-IN");
    lv_label_set_long_mode(ui->screen_Confirm_label_5, LV_LABEL_LONG_WRAP);

    //Write style for screen_Confirm_label_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Confirm_label_5, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Confirm_label_5, &lv_font_montserratMedium_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Confirm_label_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Confirm_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Confirm_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Confirm_label_5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_label_5, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_cont_btns
    ui->screen_Confirm_cont_btns = lv_obj_create(ui->screen_Confirm);
    lv_obj_set_pos(ui->screen_Confirm_cont_btns, 30, 140);
    lv_obj_set_size(ui->screen_Confirm_cont_btns, 420, 125);
    lv_obj_set_scrollbar_mode(ui->screen_Confirm_cont_btns, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Confirm_cont_btns, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_cont_btns, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_Confirm_cont_btns, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_Confirm_cont_btns, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_Confirm_cont_btns, LV_BORDER_SIDE_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_cont_btns, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_cont_btns, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_cont_btns, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_cont_btns, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_cont_btns, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_cont_btns, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_cont_btns, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_btn_finger_check
    ui->screen_Confirm_btn_finger_check = lv_button_create(ui->screen_Confirm_cont_btns);
    lv_obj_set_pos(ui->screen_Confirm_btn_finger_check, 220, 0);
    lv_obj_set_size(ui->screen_Confirm_btn_finger_check, 200, 125);
    ui->screen_Confirm_btn_finger_check_label = lv_label_create(ui->screen_Confirm_btn_finger_check);
    lv_label_set_text(ui->screen_Confirm_btn_finger_check_label, "");
    lv_label_set_long_mode(ui->screen_Confirm_btn_finger_check_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_Confirm_btn_finger_check_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_Confirm_btn_finger_check, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_Confirm_btn_finger_check_label, LV_PCT(100));

    //Write style for screen_Confirm_btn_finger_check, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Confirm_btn_finger_check, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Confirm_btn_finger_check, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Confirm_btn_finger_check, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Confirm_btn_finger_check, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_btn_finger_check, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_btn_finger_check, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui->screen_Confirm_btn_finger_check, &_finger_RGB565A8_200x125, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_opa(ui->screen_Confirm_btn_finger_check, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_recolor_opa(ui->screen_Confirm_btn_finger_check, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Confirm_btn_finger_check, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Confirm_btn_finger_check, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Confirm_btn_finger_check, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Confirm_btn_finger_check, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_btn_face_check
    ui->screen_Confirm_btn_face_check = lv_button_create(ui->screen_Confirm_cont_btns);
    lv_obj_set_pos(ui->screen_Confirm_btn_face_check, 0, 0);
    lv_obj_set_size(ui->screen_Confirm_btn_face_check, 200, 125);
    ui->screen_Confirm_btn_face_check_label = lv_label_create(ui->screen_Confirm_btn_face_check);
    lv_label_set_text(ui->screen_Confirm_btn_face_check_label, "");
    lv_label_set_long_mode(ui->screen_Confirm_btn_face_check_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_Confirm_btn_face_check_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_Confirm_btn_face_check, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_Confirm_btn_face_check_label, LV_PCT(100));

    //Write style for screen_Confirm_btn_face_check, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Confirm_btn_face_check, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Confirm_btn_face_check, lv_color_hex(0x1890FF), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Confirm_btn_face_check, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Confirm_btn_face_check, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_btn_face_check, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_btn_face_check, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui->screen_Confirm_btn_face_check, &_face_RGB565A8_200x125, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_opa(ui->screen_Confirm_btn_face_check, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_recolor_opa(ui->screen_Confirm_btn_face_check, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Confirm_btn_face_check, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Confirm_btn_face_check, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Confirm_btn_face_check, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Confirm_btn_face_check, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_control_bar
    ui->screen_Confirm_control_bar = lv_obj_create(ui->screen_Confirm);
    lv_obj_set_pos(ui->screen_Confirm_control_bar, 0, 290);
    lv_obj_set_size(ui->screen_Confirm_control_bar, 480, 30);
    lv_obj_set_scrollbar_mode(ui->screen_Confirm_control_bar, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Confirm_control_bar, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_control_bar, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_Confirm_control_bar, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_Confirm_control_bar, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_Confirm_control_bar, LV_BORDER_SIDE_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_control_bar, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Confirm_control_bar, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Confirm_control_bar, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_btn_return
    ui->screen_Confirm_btn_return = lv_button_create(ui->screen_Confirm_control_bar);
    lv_obj_set_pos(ui->screen_Confirm_btn_return, 30, 0);
    lv_obj_set_size(ui->screen_Confirm_btn_return, 100, 30);
    ui->screen_Confirm_btn_return_label = lv_label_create(ui->screen_Confirm_btn_return);
    lv_label_set_text(ui->screen_Confirm_btn_return_label, "CANCLE/返回");
    lv_label_set_long_mode(ui->screen_Confirm_btn_return_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_Confirm_btn_return_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_Confirm_btn_return, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_Confirm_btn_return_label, LV_PCT(100));

    //Write style for screen_Confirm_btn_return, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Confirm_btn_return, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Confirm_btn_return, lv_color_hex(0xc56e00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Confirm_btn_return, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Confirm_btn_return, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_btn_return, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_btn_return, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Confirm_btn_return, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Confirm_btn_return, &lv_font_SourceHanSerifSC_Regular_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Confirm_btn_return, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Confirm_btn_return, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_status_bar
    ui->screen_Confirm_status_bar = lv_obj_create(ui->screen_Confirm);
    lv_obj_set_pos(ui->screen_Confirm_status_bar, 0, 0);
    lv_obj_set_size(ui->screen_Confirm_status_bar, 480, 30);
    lv_obj_set_scrollbar_mode(ui->screen_Confirm_status_bar, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Confirm_status_bar, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_status_bar, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_Confirm_status_bar, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_Confirm_status_bar, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_Confirm_status_bar, LV_BORDER_SIDE_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_status_bar, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Confirm_status_bar, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Confirm_status_bar, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_label_time
    ui->screen_Confirm_label_time = lv_label_create(ui->screen_Confirm_status_bar);
    lv_obj_set_pos(ui->screen_Confirm_label_time, 0, 0);
    lv_obj_set_size(ui->screen_Confirm_label_time, 100, 30);
    lv_label_set_text(ui->screen_Confirm_label_time, "08:52:27");
    lv_label_set_long_mode(ui->screen_Confirm_label_time, LV_LABEL_LONG_WRAP);

    //Write style for screen_Confirm_label_time, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Confirm_label_time, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Confirm_label_time, &lv_font_montserratMedium_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Confirm_label_time, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Confirm_label_time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_cont_2
    ui->screen_Confirm_cont_2 = lv_obj_create(ui->screen_Confirm_status_bar);
    lv_obj_set_pos(ui->screen_Confirm_cont_2, 330, 0);
    lv_obj_set_size(ui->screen_Confirm_cont_2, 150, 30);
    lv_obj_set_scrollbar_mode(ui->screen_Confirm_cont_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Confirm_cont_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_cont_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_Confirm_cont_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_Confirm_cont_2, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_Confirm_cont_2, LV_BORDER_SIDE_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_label_2
    ui->screen_Confirm_label_2 = lv_label_create(ui->screen_Confirm_cont_2);
    lv_obj_set_pos(ui->screen_Confirm_label_2, 0, 0);
    lv_obj_set_size(ui->screen_Confirm_label_2, 30, 30);
    lv_label_set_text(ui->screen_Confirm_label_2, "" LV_SYMBOL_WIFI "");
    lv_label_set_long_mode(ui->screen_Confirm_label_2, LV_LABEL_LONG_WRAP);

    //Write style for screen_Confirm_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Confirm_label_2, lv_color_hex(0x52C41A), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Confirm_label_2, &lv_font_montserratMedium_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Confirm_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Confirm_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Confirm_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Confirm_label_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_label_2, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Confirm_label_1
    ui->screen_Confirm_label_1 = lv_label_create(ui->screen_Confirm_cont_2);
    lv_obj_set_pos(ui->screen_Confirm_label_1, 50, 0);
    lv_obj_set_size(ui->screen_Confirm_label_1, 100, 30);
    lv_label_set_text(ui->screen_Confirm_label_1, "DEVICE OK");
    lv_label_set_long_mode(ui->screen_Confirm_label_1, LV_LABEL_LONG_WRAP);

    //Write style for screen_Confirm_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Confirm_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Confirm_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Confirm_label_1, lv_color_hex(0x1890FF), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Confirm_label_1, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Confirm_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Confirm_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Confirm_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Confirm_label_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Confirm_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Confirm_label_1, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Confirm_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Confirm_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Confirm_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Confirm_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen_Confirm.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_Confirm);

}
