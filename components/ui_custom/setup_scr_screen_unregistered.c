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



void setup_scr_screen_unregistered(lv_ui *ui)
{
    //Write codes screen_unregistered
    ui->screen_unregistered = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_unregistered, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_unregistered, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_unregistered, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_unregistered, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_unregistered, lv_color_hex(0xFFFBE6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_unregistered, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_control_bar
    ui->screen_unregistered_control_bar = lv_obj_create(ui->screen_unregistered);
    lv_obj_set_pos(ui->screen_unregistered_control_bar, 0, 290);
    lv_obj_set_size(ui->screen_unregistered_control_bar, 480, 30);
    lv_obj_set_scrollbar_mode(ui->screen_unregistered_control_bar, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_unregistered_control_bar, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_control_bar, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_unregistered_control_bar, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_unregistered_control_bar, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_unregistered_control_bar, LV_BORDER_SIDE_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_control_bar, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_unregistered_control_bar, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_unregistered_control_bar, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_unregistered_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_unregistered_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_unregistered_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_unregistered_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_control_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_btn_return
    ui->screen_unregistered_btn_return = lv_button_create(ui->screen_unregistered_control_bar);
    lv_obj_set_pos(ui->screen_unregistered_btn_return, 30, -1);
    lv_obj_set_size(ui->screen_unregistered_btn_return, 100, 30);
    ui->screen_unregistered_btn_return_label = lv_label_create(ui->screen_unregistered_btn_return);
    lv_label_set_text(ui->screen_unregistered_btn_return_label, "CANCLE/返回");
    lv_label_set_long_mode(ui->screen_unregistered_btn_return_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_unregistered_btn_return_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_unregistered_btn_return, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_unregistered_btn_return_label, LV_PCT(100));

    //Write style for screen_unregistered_btn_return, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_btn_return, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_btn_return, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_btn_return, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_unregistered_btn_return, lv_color_hex(0xc56e00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_unregistered_btn_return, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_btn_return, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_unregistered_btn_return, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_unregistered_btn_return, &lv_font_SourceHanSerifSC_Regular_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_unregistered_btn_return, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_unregistered_btn_return, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_status_bar
    ui->screen_unregistered_status_bar = lv_obj_create(ui->screen_unregistered);
    lv_obj_set_pos(ui->screen_unregistered_status_bar, 0, 0);
    lv_obj_set_size(ui->screen_unregistered_status_bar, 480, 30);
    lv_obj_set_scrollbar_mode(ui->screen_unregistered_status_bar, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_unregistered_status_bar, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_status_bar, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_unregistered_status_bar, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_unregistered_status_bar, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_unregistered_status_bar, LV_BORDER_SIDE_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_status_bar, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_unregistered_status_bar, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_unregistered_status_bar, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_unregistered_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_unregistered_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_unregistered_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_unregistered_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_status_bar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_label_time
    ui->screen_unregistered_label_time = lv_label_create(ui->screen_unregistered_status_bar);
    lv_obj_set_pos(ui->screen_unregistered_label_time, 0, 0);
    lv_obj_set_size(ui->screen_unregistered_label_time, 100, 30);
    lv_label_set_text(ui->screen_unregistered_label_time, "08:52:27");
    lv_label_set_long_mode(ui->screen_unregistered_label_time, LV_LABEL_LONG_WRAP);

    //Write style for screen_unregistered_label_time, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_unregistered_label_time, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_unregistered_label_time, &lv_font_montserratMedium_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_unregistered_label_time, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_unregistered_label_time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_cont_6
    ui->screen_unregistered_cont_6 = lv_obj_create(ui->screen_unregistered_status_bar);
    lv_obj_set_pos(ui->screen_unregistered_cont_6, 330, 0);
    lv_obj_set_size(ui->screen_unregistered_cont_6, 150, 30);
    lv_obj_set_scrollbar_mode(ui->screen_unregistered_cont_6, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_unregistered_cont_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_cont_6, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_unregistered_cont_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_unregistered_cont_6, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_unregistered_cont_6, LV_BORDER_SIDE_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_cont_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_cont_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_unregistered_cont_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_unregistered_cont_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_unregistered_cont_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_unregistered_cont_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_cont_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_label_26
    ui->screen_unregistered_label_26 = lv_label_create(ui->screen_unregistered_cont_6);
    lv_obj_set_pos(ui->screen_unregistered_label_26, 0, 0);
    lv_obj_set_size(ui->screen_unregistered_label_26, 30, 30);
    lv_label_set_text(ui->screen_unregistered_label_26, "" LV_SYMBOL_WIFI "");
    lv_label_set_long_mode(ui->screen_unregistered_label_26, LV_LABEL_LONG_WRAP);

    //Write style for screen_unregistered_label_26, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_label_26, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_label_26, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_unregistered_label_26, lv_color_hex(0x52C41A), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_unregistered_label_26, &lv_font_montserratMedium_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_unregistered_label_26, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_unregistered_label_26, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_unregistered_label_26, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_unregistered_label_26, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_label_26, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_unregistered_label_26, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_unregistered_label_26, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_unregistered_label_26, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_unregistered_label_26, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_label_26, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_label_25
    ui->screen_unregistered_label_25 = lv_label_create(ui->screen_unregistered_cont_6);
    lv_obj_set_pos(ui->screen_unregistered_label_25, 50, 0);
    lv_obj_set_size(ui->screen_unregistered_label_25, 100, 30);
    lv_label_set_text(ui->screen_unregistered_label_25, "DEVICE OK");
    lv_label_set_long_mode(ui->screen_unregistered_label_25, LV_LABEL_LONG_WRAP);

    //Write style for screen_unregistered_label_25, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_label_25, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_label_25, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_unregistered_label_25, lv_color_hex(0x1890FF), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_unregistered_label_25, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_unregistered_label_25, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_unregistered_label_25, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_unregistered_label_25, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_unregistered_label_25, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_label_25, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_unregistered_label_25, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_unregistered_label_25, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_unregistered_label_25, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_unregistered_label_25, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_label_25, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_label_7
    ui->screen_unregistered_label_7 = lv_label_create(ui->screen_unregistered);
    lv_obj_set_pos(ui->screen_unregistered_label_7, 165, 258);
    lv_obj_set_size(ui->screen_unregistered_label_7, 150, 32);
    lv_label_set_text(ui->screen_unregistered_label_7, "3s后自动返回主页");
    lv_label_set_long_mode(ui->screen_unregistered_label_7, LV_LABEL_LONG_WRAP);

    //Write style for screen_unregistered_label_7, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_unregistered_label_7, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_unregistered_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_unregistered_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_unregistered_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_unregistered_label_7, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_unregistered_label_7, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_unregistered_label_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_unregistered_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_unregistered_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_unregistered_label_7, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_img_1
    ui->screen_unregistered_img_1 = lv_image_create(ui->screen_unregistered);
    lv_obj_set_pos(ui->screen_unregistered_img_1, 190, 30);
    lv_obj_set_size(ui->screen_unregistered_img_1, 100, 100);
    lv_obj_add_flag(ui->screen_unregistered_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->screen_unregistered_img_1, &_warning_RGB565A8_100x100);
    lv_image_set_pivot(ui->screen_unregistered_img_1, 50,50);
    lv_image_set_rotation(ui->screen_unregistered_img_1, 0);

    //Write style for screen_unregistered_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->screen_unregistered_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->screen_unregistered_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_label_Card
    ui->screen_unregistered_label_Card = lv_label_create(ui->screen_unregistered);
    lv_obj_set_pos(ui->screen_unregistered_label_Card, 143, 130);
    lv_obj_set_size(ui->screen_unregistered_label_Card, 204, 32);
    lv_label_set_text(ui->screen_unregistered_label_Card, "Card  UID:AABBCCDD");
    lv_label_set_long_mode(ui->screen_unregistered_label_Card, LV_LABEL_LONG_WRAP);

    //Write style for screen_unregistered_label_Card, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_unregistered_label_Card, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_unregistered_label_Card, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_unregistered_label_Card, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_unregistered_label_Card, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_label_Card, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_unregistered_label_28
    ui->screen_unregistered_label_28 = lv_label_create(ui->screen_unregistered);
    lv_obj_set_pos(ui->screen_unregistered_label_28, 146, 157);
    lv_obj_set_size(ui->screen_unregistered_label_28, 199, 32);
    lv_label_set_text(ui->screen_unregistered_label_28, "未注册/需绑定");
    lv_label_set_long_mode(ui->screen_unregistered_label_28, LV_LABEL_LONG_WRAP);

    //Write style for screen_unregistered_label_28, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_unregistered_label_28, lv_color_hex(0x2d165a), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_unregistered_label_28, &lv_font_SourceHanSerifSC_Regular_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_unregistered_label_28, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_unregistered_label_28, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_unregistered_label_28, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen_unregistered.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_unregistered);

}
