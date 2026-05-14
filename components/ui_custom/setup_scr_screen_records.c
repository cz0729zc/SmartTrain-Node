#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"

void setup_scr_screen_records(lv_ui *ui)
{
    ui->screen_records = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_records, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_records, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ui->screen_records, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_records, lv_color_hex(0xF0F2F5), LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_status_bar = lv_obj_create(ui->screen_records);
    lv_obj_set_pos(ui->screen_records_status_bar, 0, 0);
    lv_obj_set_size(ui->screen_records_status_bar, 480, 30);
    lv_obj_set_scrollbar_mode(ui->screen_records_status_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(ui->screen_records_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_records_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_records_status_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_records_status_bar, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->screen_records_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_label_time = lv_label_create(ui->screen_records_status_bar);
    lv_obj_set_pos(ui->screen_records_label_time, 0, 0);
    lv_obj_set_size(ui->screen_records_label_time, 100, 30);
    lv_label_set_text(ui->screen_records_label_time, "--:--:--");
    lv_obj_set_style_text_color(ui->screen_records_label_time, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_records_label_time, &lv_font_montserratMedium_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_records_label_time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_cont_status = lv_obj_create(ui->screen_records_status_bar);
    lv_obj_set_pos(ui->screen_records_cont_status, 330, 0);
    lv_obj_set_size(ui->screen_records_cont_status, 150, 30);
    lv_obj_set_scrollbar_mode(ui->screen_records_cont_status, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(ui->screen_records_cont_status, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_records_cont_status, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_records_cont_status, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->screen_records_cont_status, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_label_wifi_icon = lv_label_create(ui->screen_records_cont_status);
    lv_obj_set_pos(ui->screen_records_label_wifi_icon, 0, 0);
    lv_obj_set_size(ui->screen_records_label_wifi_icon, 30, 30);
    lv_label_set_text(ui->screen_records_label_wifi_icon, "" LV_SYMBOL_WIFI "");
    lv_obj_set_style_text_color(ui->screen_records_label_wifi_icon, lv_color_hex(0xBFBFBF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_records_label_wifi_icon, &lv_font_montserratMedium_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_records_label_wifi_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_records_label_wifi_icon, 3, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_label_wifi_text = lv_label_create(ui->screen_records_cont_status);
    lv_obj_set_pos(ui->screen_records_label_wifi_text, 50, 0);
    lv_obj_set_size(ui->screen_records_label_wifi_text, 100, 30);
    lv_label_set_text(ui->screen_records_label_wifi_text, "WiFi OFF");
    lv_obj_set_style_text_color(ui->screen_records_label_wifi_text, lv_color_hex(0x8C8C8C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_records_label_wifi_text, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_records_label_wifi_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_records_label_wifi_text, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_label_title = lv_label_create(ui->screen_records);
    lv_obj_set_pos(ui->screen_records_label_title, 0, 42);
    lv_obj_set_size(ui->screen_records_label_title, 480, 32);
    lv_label_set_text(ui->screen_records_label_title, "Attendance Records");
    lv_obj_set_style_text_color(ui->screen_records_label_title, lv_color_hex(0x1F1F1F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_records_label_title, &lv_font_montserratMedium_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_records_label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_cont_body = lv_obj_create(ui->screen_records);
    lv_obj_set_pos(ui->screen_records_cont_body, 20, 84);
    lv_obj_set_size(ui->screen_records_cont_body, 440, 166);
    lv_obj_set_scrollbar_mode(ui->screen_records_cont_body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(ui->screen_records_cont_body, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_records_cont_body, lv_color_hex(0xD9D9D9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_records_cont_body, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_records_cont_body, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_records_cont_body, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->screen_records_cont_body, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_label_body = lv_label_create(ui->screen_records_cont_body);
    lv_obj_set_pos(ui->screen_records_label_body, 12, 8);
    lv_obj_set_size(ui->screen_records_label_body, 416, 150);
    lv_label_set_text(ui->screen_records_label_body, "No records yet");
    lv_label_set_long_mode(ui->screen_records_label_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(ui->screen_records_label_body, lv_color_hex(0x262626), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_records_label_body, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_records_label_body, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_label_status = lv_label_create(ui->screen_records);
    lv_obj_set_pos(ui->screen_records_label_status, 20, 256);
    lv_obj_set_size(ui->screen_records_label_status, 440, 30);
    lv_label_set_text(ui->screen_records_label_status, "/records/attendance.csv");
    lv_obj_set_style_text_color(ui->screen_records_label_status, lv_color_hex(0x595959), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_records_label_status, &lv_font_montserratMedium_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_records_label_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_records_label_status, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_control_bar = lv_obj_create(ui->screen_records);
    lv_obj_set_pos(ui->screen_records_control_bar, 0, 290);
    lv_obj_set_size(ui->screen_records_control_bar, 480, 30);
    lv_obj_set_scrollbar_mode(ui->screen_records_control_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(ui->screen_records_control_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_records_control_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_records_control_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_records_control_bar, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->screen_records_control_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_btn_return = lv_button_create(ui->screen_records_control_bar);
    lv_obj_set_pos(ui->screen_records_btn_return, 30, 0);
    lv_obj_set_size(ui->screen_records_btn_return, 100, 30);
    lv_obj_clear_flag(ui->screen_records_btn_return, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui->screen_records_btn_return, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_records_btn_return, lv_color_hex(0xC56E00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_records_btn_return, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_records_btn_return, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_records_btn_return_label = lv_label_create(ui->screen_records_btn_return);
    lv_label_set_text(ui->screen_records_btn_return_label, "BACK");
    lv_obj_set_style_text_color(ui->screen_records_btn_return_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_records_btn_return_label, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(ui->screen_records_btn_return_label);

    lv_obj_update_layout(ui->screen_records);
}
