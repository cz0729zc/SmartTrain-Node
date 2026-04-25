#include "nvs_flash.h"
#include "esp_log.h"
#include "sensor_data.h"
#include "app_sensor.h"
#include "app_network.h"
#include "app_rfid.h"
#include "app_co2.h"
#include "app_test_hub.h"
#include "attendance_profile.h"
#include "app_lvgl.h"
#include "app_face.h"
#include "app_fingerprint.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* GUI Guider 移植*/
#include "gui_guider.h"
#include "custom.h"
#include "events_init.h"

lv_ui guider_ui;

static const char *TAG = "main";

#define FM225_UART_NUM      UART_NUM_1
#define FM225_UART_TX_GPIO  GPIO_NUM_11
#define FM225_UART_RX_GPIO  GPIO_NUM_10
#define FM225_UART_BAUD     115200

/* 管理员卡 UID（按实际管理员卡 UID 修改） */
#define ADMIN_CARD_UID      "04:11:22:33:44"

static bool s_rfid_ready = false;

static bool s_face_ready = false;
static bool s_finger_ready = false;
static bool s_has_active_profile = false;
static bool s_admin_mode = false;
static attendance_profile_t s_active_profile = {0};
static SemaphoreHandle_t s_bio_mutex = NULL;
static char s_admin_target_sid[ATTENDANCE_STUDENT_ID_MAX_LEN] = {0};

static bool is_admin_card(const char *uid_str)
{
    return uid_str != NULL && strcmp(uid_str, ADMIN_CARD_UID) == 0;
}

static void ui_async_show_admin(void *arg)
{
    (void)arg;
    if (guider_ui.screen_admin_del) {
        setup_scr_screen_admin(&guider_ui);
    }

    if (s_admin_target_sid[0] != '\0' && guider_ui.screen_admin_label_9 != NULL) {
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "学员:%s", s_admin_target_sid);
        lv_label_set_text(guider_ui.screen_admin_label_9, buf);
    }

    lv_screen_load(guider_ui.screen_admin);
}


static void format_time_text(char *out, size_t out_size)
{
    if (out == NULL || out_size < 9) {
        return;
    }

    uint64_t sec = (uint64_t)(esp_timer_get_time() / 1000000LL);
    uint32_t hh = (uint32_t)((sec / 3600ULL) % 24ULL);
    uint32_t mm = (uint32_t)((sec / 60ULL) % 60ULL);
    uint32_t ss = (uint32_t)(sec % 60ULL);
    snprintf(out, out_size, "%02u:%02u:%02u", (unsigned)hh, (unsigned)mm, (unsigned)ss);
}

static void extract_uid_suffix(const char *uid_str, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }

    out[0] = '\0';
    if (uid_str == NULL || uid_str[0] == '\0') {
        strlcpy(out, "----", out_size);
        return;
    }

    char rev[5] = {0};
    int count = 0;
    for (int i = (int)strlen(uid_str) - 1; i >= 0 && count < 4; --i) {
        if (isxdigit((unsigned char)uid_str[i])) {
            rev[count++] = uid_str[i];
        }
    }

    if (count == 0) {
        strlcpy(out, "----", out_size);
        return;
    }

    for (int i = 0; i < count; ++i) {
        out[i] = rev[count - 1 - i];
    }
    out[count] = '\0';
}


/*
 * main.c 只负责业务入口编排：
 * - 系统初始化
 * - 业务模块启动顺序
 * - 绑定回调
 * 所有测试/自检逻辑统一收敛到 app_test_hub。
 */

void app_main(void)
{
    // 1. 系统级初始化 (NVS, Log, BSP...)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "开始自检 RFID 模块...");
    s_rfid_ready = (app_rfid_probe(250) == ESP_OK);
    if (s_rfid_ready) {
        ESP_LOGI(TAG, "RFID 模块自检通过");
    } else {
        ESP_LOGW(TAG, "RFID 模块自检失败");
    }

    ESP_LOGI(TAG, "开始自检人脸模块...");
    app_face_config_t face_cfg = {
        .uart_num = FM225_UART_NUM,
        .tx_gpio = FM225_UART_TX_GPIO,
        .rx_gpio = FM225_UART_RX_GPIO,
        .baud_rate = FM225_UART_BAUD,
        .cmd_timeout_ms = 3000,
    };
    if (app_face_init(&face_cfg) == ESP_OK && app_face_wait_ready(1000) == ESP_OK) {
        s_face_ready = true;
        ESP_LOGI(TAG, "人脸模块自检通过");
    } else {
        s_face_ready = false;
        ESP_LOGW(TAG, "人脸模块自检失败");
    }

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

    ESP_ERROR_CHECK(app_lvgl_init());
    ESP_LOGI(TAG, "初始化 Guider UI...");

    /* app_main 任务中调用 LVGL API 需要先加锁，避免与 taskLVGL 并发访问 */
    lvgl_port_lock(0);
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    events_selfcheck_set_result(EVENTS_SC_RFID,
                                s_rfid_ready,
                                s_rfid_ready ? "RFID module ready" : "RFID module failed");
    events_selfcheck_set_result(EVENTS_SC_FACE,
                                s_face_ready,
                                s_face_ready ? "Face module ready" : "Face module failed");
    events_selfcheck_set_result(EVENTS_SC_FINGER,
                                s_finger_ready,
                                s_finger_ready ? "Finger module ready" : "Finger module failed");
    events_selfcheck_finish();
    lvgl_port_unlock();

    ESP_LOGI(TAG, "app_main 执行完毕");
}
