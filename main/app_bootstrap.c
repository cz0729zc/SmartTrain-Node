#include "app_bootstrap.h"

#include "app_attendance.h"
#include "app_face.h"
#include "app_fingerprint.h"
#include "app_lvgl.h"
#include "app_network.h"
#include "app_rfid.h"
#include "app_sensor.h"
#include "app_wifi.h"
#include "attendance_profile.h"
#include "events_init.h"
#include "gui_guider.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>

lv_ui guider_ui;

static const char *TAG = "app_bootstrap";

#define FM225_UART_NUM      UART_NUM_1
#define FM225_UART_TX_GPIO  GPIO_NUM_11
#define FM225_UART_RX_GPIO  GPIO_NUM_10
#define FM225_UART_BAUD     115200

/* Set to 1 for one debug boot, then set back to 0 before normal demos. */
#define CLEAR_BIOMETRIC_DB_ON_BOOT 1

/*
 * Set to 1 to boot into an LVGL touch hit-test page only.
 * The test places buttons at the four corners and center of the 480x320
 * logical landscape screen.
 */
#define RUN_LVGL_TOUCH_BUTTON_TEST 0
#define NETWORK_SELFCHECK_TIMEOUT_MS 10000
#define NETWORK_SELFCHECK_POLL_MS    200

static bool s_rfid_ready = false;
static bool s_face_ready = false;
static bool s_finger_ready = false;
static bool s_sensor_ready = false;
static bool s_network_started = false;
static lv_obj_t *s_touch_info_label = NULL;

static void log_system_status(const char *stage)
{
    if (stage == NULL) {
        stage = "unknown";
    }

    uint32_t free_heap = esp_get_free_heap_size();
    size_t free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t total_8bit = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t free_dma = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t total_dma = heap_caps_get_total_size(MALLOC_CAP_DMA);

    ESP_LOGI(TAG,
             "[%s] core=%d free_heap=%u 8bit=%u/%u internal=%u/%u dma=%u/%u",
             stage,
             (int)xPortGetCoreID(),
             (unsigned)free_heap,
             (unsigned)free_8bit,
             (unsigned)total_8bit,
             (unsigned)free_internal,
             (unsigned)total_internal,
             (unsigned)free_dma,
             (unsigned)total_dma);
}

static bool probe_face_module(const app_face_config_t *face_cfg)
{
    if (app_face_init(face_cfg) != ESP_OK) {
        return false;
    }

    esp_err_t ready_ret = app_face_wait_ready(3000);
    if (ready_ret == ESP_OK) {
        return true;
    }

    uint8_t result_code = 0xFF;
    driver_fm225_status_t status = DRIVER_FM225_STATUS_INVALID;
    esp_err_t status_ret = app_face_get_status(&status, &result_code);
    if (status_ret == ESP_OK && result_code == DRIVER_FM225_RESULT_SUCCESS) {
        ESP_LOGW(TAG,
                 "FM225 ready note missed, status probe passed status=%u",
                 (unsigned)status);
        return true;
    }

    ESP_LOGW(TAG,
             "FM225 probe failed ready=%s status=%s result=0x%02X(%s)",
             esp_err_to_name(ready_ret),
             esp_err_to_name(status_ret),
             (unsigned)result_code,
             app_face_result_message(result_code));
    return false;
}

static void clear_biometric_databases_on_boot(void)
{
#if CLEAR_BIOMETRIC_DB_ON_BOOT
    uint8_t face_result = 0xFF;
    uint8_t finger_confirm = 0xFF;

    ESP_LOGW(TAG, "CLEAR_BIOMETRIC_DB_ON_BOOT enabled: clearing FM225/AS608 templates");

    if (s_face_ready) {
        esp_err_t ret = app_face_delete_all(&face_result);
        if (ret == ESP_OK && face_result == DRIVER_FM225_RESULT_SUCCESS) {
            ESP_LOGW(TAG, "FM225 face database cleared");
        } else {
            ESP_LOGE(TAG,
                     "FM225 face database clear failed ret=%s result=0x%02X(%s)",
                     esp_err_to_name(ret),
                     (unsigned)face_result,
                     app_face_result_message(face_result));
        }
    } else {
        ESP_LOGW(TAG, "skip FM225 clear: face module not ready");
    }

    if (s_finger_ready) {
        esp_err_t ret = app_fingerprint_empty(&finger_confirm);
        if (ret == ESP_OK && finger_confirm == DRIVER_AS608_CONFIRM_OK) {
            ESP_LOGW(TAG, "AS608 fingerprint database cleared");
        } else {
            ESP_LOGE(TAG,
                     "AS608 fingerprint database clear failed ret=%s confirm=0x%02X(%s)",
                     esp_err_to_name(ret),
                     (unsigned)finger_confirm,
                     app_fingerprint_confirm_message(finger_confirm));
        }
    } else {
        ESP_LOGW(TAG, "skip AS608 clear: fingerprint module not ready");
    }

    esp_err_t ret = attendance_profile_init();
    if (ret == ESP_OK) {
        ret = attendance_profile_clear_biometric_bindings();
        if (ret == ESP_OK) {
            attendance_profile_dump();
        } else {
            ESP_LOGE(TAG, "clear local biometric bindings failed: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "attendance profile init before clear failed: %s", esp_err_to_name(ret));
    }
#endif
}

static void lvgl_touch_button_test_event_cb(lv_event_t *e)
{
    if (e == NULL) {
        return;
    }

    const char *name = (const char *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) {
        indev = lv_indev_get_act();
    }

    if (indev == NULL) {
        ESP_LOGI(TAG, "LVGL touch: indev=NULL");
        return;
    }

    lv_point_t point = {0};
    lv_indev_get_point(indev, &point);
    ESP_LOGI(TAG, "LVGL touch button=%s event=%d x=%d y=%d",
             name ? name : "unknown", (int)code, (int)point.x, (int)point.y);

    if (s_touch_info_label != NULL) {
        char buf[96];
        snprintf(buf, sizeof(buf), "%s: x=%d y=%d",
                 name ? name : "touch", (int)point.x, (int)point.y);
        lv_label_set_text(s_touch_info_label, buf);
    }
}

static void create_touch_test_button(lv_obj_t *parent,
                                     const char *name,
                                     lv_align_t align,
                                     int32_t x_ofs,
                                     int32_t y_ofs)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 112, 56);
    lv_obj_align(btn, align, x_ofs, y_ofs);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, lvgl_touch_button_test_event_cb, LV_EVENT_PRESSED, (void *)name);
    lv_obj_add_event_cb(btn, lvgl_touch_button_test_event_cb, LV_EVENT_CLICKED, (void *)name);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, name);
    lv_obj_center(label);

    lv_area_t area;
    lv_obj_get_coords(btn, &area);
    ESP_LOGI(TAG, "Touch test button %s area: x1=%d y1=%d x2=%d y2=%d",
             name, (int)area.x1, (int)area.y1, (int)area.x2, (int)area.y2);
}

static void create_lvgl_touch_test_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x102030), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL Touch Coordinate Test");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    s_touch_info_label = lv_label_create(scr);
    lv_label_set_text(s_touch_info_label, "Press a button");
    lv_obj_set_style_text_color(s_touch_info_label, lv_color_hex(0x7FDBFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_touch_info_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_touch_info_label, LV_ALIGN_CENTER, 0, 48);

    create_touch_test_button(scr, "TOP LEFT", LV_ALIGN_TOP_LEFT, 8, 56);
    create_touch_test_button(scr, "TOP RIGHT", LV_ALIGN_TOP_RIGHT, -8, 56);
    create_touch_test_button(scr, "BOTTOM LEFT", LV_ALIGN_BOTTOM_LEFT, 8, -8);
    create_touch_test_button(scr, "BOTTOM RIGHT", LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    create_touch_test_button(scr, "CENTER", LV_ALIGN_CENTER, 0, -24);

    lv_screen_load(scr);
}

static void admin_card_write_mode_cb(void *user_data)
{
    (void)user_data;
    esp_err_t ret = app_attendance_enter_card_write_mode();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "enter card write mode failed: %s", esp_err_to_name(ret));
        events_show_admin_status("Write mode failed");
    }
}

static void admin_return_cb(void *user_data)
{
    (void)user_data;
    (void)app_attendance_exit_admin_mode();
    events_show_standby();
}

static void admin_face_register_cb(void *user_data)
{
    (void)user_data;
    esp_err_t ret = app_attendance_register_face_selected();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "queue face register failed: %s", esp_err_to_name(ret));
        events_show_admin_status("Face register failed");
    }
}

static void admin_finger_register_cb(void *user_data)
{
    (void)user_data;
    esp_err_t ret = app_attendance_register_fingerprint_selected();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "queue finger register failed: %s", esp_err_to_name(ret));
        events_show_admin_status("Finger register failed");
    }
}

static void admin_records_cb(void *user_data)
{
    (void)user_data;
    esp_err_t ret = app_attendance_show_records();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "queue show records failed: %s", esp_err_to_name(ret));
        events_show_admin_status("Records read failed");
    }
}

static void records_clear_cb(void *user_data)
{
    (void)user_data;
    esp_err_t ret = app_attendance_clear_records();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "queue clear records failed: %s", esp_err_to_name(ret));
    }
}

static void confirm_return_cb(void *user_data)
{
    (void)user_data;
    esp_err_t ret = app_attendance_confirm_return();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "queue confirm return failed: %s", esp_err_to_name(ret));
        events_show_standby();
    }
}

static void confirm_face_check_cb(void *user_data)
{
    (void)user_data;
    esp_err_t ret = app_attendance_verify_face_selected();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "queue face verify failed: %s", esp_err_to_name(ret));
        events_show_confirm_status("Face verify failed");
    }
}

static void confirm_finger_check_cb(void *user_data)
{
    (void)user_data;
    esp_err_t ret = app_attendance_verify_fingerprint_selected();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "queue finger verify failed: %s", esp_err_to_name(ret));
        events_show_confirm_status("Finger verify failed");
    }
}

static void register_ui_callbacks(void)
{
    events_set_admin_card_write_callback(admin_card_write_mode_cb, NULL);
    events_set_admin_return_callback(admin_return_cb, NULL);
    events_set_admin_face_register_callback(admin_face_register_cb, NULL);
    events_set_admin_finger_register_callback(admin_finger_register_cb, NULL);
    events_set_admin_records_callback(admin_records_cb, NULL);
    events_set_records_clear_callback(records_clear_cb, NULL);
    events_set_confirm_return_callback(confirm_return_cb, NULL);
    events_set_confirm_face_check_callback(confirm_face_check_cb, NULL);
    events_set_confirm_finger_check_callback(confirm_finger_check_cb, NULL);
}

static esp_err_t start_network_module(void)
{
    esp_err_t ret = app_network_start();
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        s_network_started = true;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "network task start failed: %s", esp_err_to_name(ret));
    return ret;
}

static bool wait_network_connected_for_selfcheck(void)
{
    const TickType_t start_tick = xTaskGetTickCount();
    const TickType_t timeout_ticks = pdMS_TO_TICKS(NETWORK_SELFCHECK_TIMEOUT_MS);

    while ((xTaskGetTickCount() - start_tick) < timeout_ticks) {
        if (app_wifi_is_connected()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(NETWORK_SELFCHECK_POLL_MS));
    }

    return app_wifi_is_connected();
}

static void run_startup_selfcheck(void)
{
    ESP_LOGI(TAG, "开始自检 RFID 模块...");
    s_rfid_ready = (app_rfid_probe(250) == ESP_OK);
    if (s_rfid_ready) {
        ESP_LOGI(TAG, "RFID 模块自检通过");
    } else {
        ESP_LOGW(TAG, "RFID 模块自检失败");
    }

    ESP_LOGI(TAG, "UI selfcheck result: RFID begin");
    events_selfcheck_set_result(EVENTS_SC_RFID,
                                s_rfid_ready,
                                s_rfid_ready ? "RFID module ready" : "RFID module failed");

    ESP_LOGI(TAG, "开始自检人脸模块...");
    app_face_config_t face_cfg = {
        .uart_num = FM225_UART_NUM,
        .tx_gpio = FM225_UART_TX_GPIO,
        .rx_gpio = FM225_UART_RX_GPIO,
        .baud_rate = FM225_UART_BAUD,
        .cmd_timeout_ms = 3000,
    };
    if (probe_face_module(&face_cfg)) {
        s_face_ready = true;
        ESP_LOGI(TAG, "人脸模块自检通过");
    } else {
        s_face_ready = false;
        ESP_LOGW(TAG, "人脸模块自检失败");
    }

    ESP_LOGI(TAG, "UI selfcheck result: FACE begin");
    events_selfcheck_set_result(EVENTS_SC_FACE,
                                s_face_ready,
                                s_face_ready ? "Face module ready" : "Face module failed");

    ESP_LOGI(TAG, "开始自检指纹模块...");
    uint8_t finger_confirm = 0xFF;
    driver_as608_sys_para_t finger_para = {0};
    if (app_fingerprint_init() == ESP_OK
        && app_fingerprint_get_sys_para(&finger_para, &finger_confirm) == ESP_OK
        && finger_confirm == DRIVER_AS608_CONFIRM_OK) {
        s_finger_ready = true;
        ESP_LOGI(TAG, "指纹模块自检通过");
    } else {
        s_finger_ready = false;
        ESP_LOGW(TAG, "指纹模块自检失败");
    }

    ESP_LOGI(TAG, "UI selfcheck result: FINGER begin");
    events_selfcheck_set_result(EVENTS_SC_FINGER,
                                s_finger_ready,
                                s_finger_ready ? "Finger module ready" : "Finger module failed");

    ESP_LOGI(TAG, "开始自检网络连接...");
    bool network_ready = s_network_started && wait_network_connected_for_selfcheck();
    if (network_ready) {
        ESP_LOGI(TAG, "网络自检通过: WiFi connected");
    } else if (!s_network_started) {
        ESP_LOGW(TAG, "网络自检失败: network task not started");
    } else {
        ESP_LOGW(TAG, "网络自检失败: WiFi not connected within %u ms",
                 (unsigned)NETWORK_SELFCHECK_TIMEOUT_MS);
    }

    ESP_LOGI(TAG, "UI selfcheck result: NETWORK begin");
    events_selfcheck_set_result(EVENTS_SC_NETWORK,
                                network_ready,
                                network_ready ? "WiFi connected" : "WiFi not connected");

    clear_biometric_databases_on_boot();

    ESP_LOGI(TAG, "UI selfcheck finish begin");
    events_selfcheck_finish();
    ESP_LOGI(TAG, "UI selfcheck finish done");
}

static esp_err_t init_lvgl_and_guider_ui(void)
{
    ESP_RETURN_ON_ERROR(app_lvgl_init(), TAG, "LVGL init failed");
    ESP_LOGI(TAG, "初始化 Guider UI...");

#if RUN_LVGL_TOUCH_BUTTON_TEST
    ESP_LOGI(TAG, "RUN_LVGL_TOUCH_BUTTON_TEST enabled, booting touch hit-test screen only");
    lvgl_port_lock(0);
    create_lvgl_touch_test_screen();
    lvgl_port_unlock();
    return ESP_ERR_INVALID_STATE;
#endif

    lvgl_port_lock(0);
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    register_ui_callbacks();
    lvgl_port_unlock();

    return ESP_OK;
}

static void start_runtime_modules(void)
{
    esp_err_t sensor_err = app_sensor_start();
    s_sensor_ready = (sensor_err == ESP_OK || sensor_err == ESP_ERR_INVALID_STATE);
    if (!s_sensor_ready) {
        ESP_LOGW(TAG, "DHT11 sensor task start failed: %s", esp_err_to_name(sensor_err));
    }

    esp_err_t attendance_err = app_attendance_start();
    if (attendance_err != ESP_OK) {
        ESP_LOGW(TAG, "attendance start failed: %s", esp_err_to_name(attendance_err));
    }

    if (!s_network_started) {
        ESP_ERROR_CHECK(start_network_module());
    }
}

esp_err_t app_bootstrap_start(void)
{
    log_system_status("nvs_inited");

    esp_err_t ret = init_lvgl_and_guider_ui();
    if (ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_ERROR_CHECK(start_network_module());

    /* Give taskLVGL and network_task a scheduling chance before self-check polling. */
    vTaskDelay(pdMS_TO_TICKS(1));

    run_startup_selfcheck();

    start_runtime_modules();

    ESP_LOGI(TAG, "app_bootstrap 执行完毕");
    return ESP_OK;
}
