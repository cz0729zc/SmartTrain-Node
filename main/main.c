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

/* 1=启用 7 页面自动轮播测试，0=正常业务流程 */
#define RUN_UI_CYCLE_TEST   0
#define RUN_UI_GUIDER_TEST   1

/* 硬件联调开关：未接外设时先置 0，按模块逐步打开 */
#define ENABLE_FACE_MODULE     0
#define ENABLE_FINGER_MODULE   0
#define ENABLE_RFID_MODULE     0

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

    // 1.1 初始化本地学员档案映射 (UID -> 学号/姓名)    
    // ESP_ERROR_CHECK(attendance_profile_init());

    // 示例: 首次可手动写入测试档案，确认刷卡后能识别姓名
    // ESP_ERROR_CHECK(attendance_profile_upsert("04:A1:B2:C3:D4", "2026001", "张三"));

    // ESP_LOGI(TAG, "系统初始化完成，开始创建并发任务...");

    // s_bio_mutex = xSemaphoreCreateMutex();
    // if (s_bio_mutex == NULL) {
    //     ESP_LOGE(TAG, "创建生物识别互斥锁失败");
    // }

    // 集中执行可选自检与 demo 任务（由 app_test_hub.c 的 RUN_* 宏控制）
    // app_test_hub_run_startup_tests();
    // app_test_hub_start_optional_tasks();

    // 2. 初始化传感器数据队列 (用于 sensor_task 与 network_task 通信)
    // ESP_ERROR_CHECK(sensor_queue_init(5));

    // 3. 初始化 LVGL 显示模块 (LCD + 触摸屏)
    ESP_ERROR_CHECK(app_lvgl_init());

#if RUN_UI_GUIDER_TEST
    ESP_LOGI(TAG, "初始化 Guider UI...");
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    ESP_LOGI(TAG, "Guider UI 初始化完成");
    return;
#else
    app_lvgl_set_action_callback(on_ui_action, NULL);
    app_lvgl_demo();
#endif


#if RUN_UI_CYCLE_TEST
    app_lvgl_start_cycle_test(3000);
    ESP_LOGW(TAG, "当前运行在 UI 轮播测试模式：已跳过 RFID/人脸/指纹真实流程初始化");
    ESP_LOGI(TAG, "app_main 执行完毕 (即将在空闲时自行删除)");
    return;
#endif



    app_face_config_t face_cfg = {
        .uart_num = FM225_UART_NUM,
        .tx_gpio = FM225_UART_TX_GPIO,
        .rx_gpio = FM225_UART_RX_GPIO,
        .baud_rate = FM225_UART_BAUD,
        .cmd_timeout_ms = 3000,
    };

#if ENABLE_FACE_MODULE
    ESP_LOGI(TAG, "开始初始化人脸模块...");
    if (app_face_init(&face_cfg) == ESP_OK) {
        /* READY 等待不宜过长，避免主线程阻塞导致 WDT 复位 */
        if (app_face_wait_ready(1000) == ESP_OK) {
            s_face_ready = true;
            ESP_LOGI(TAG, "人脸模块 READY");
        } else {
            ESP_LOGW(TAG, "人脸模块未返回 READY，后续按需重试");
        }
    } else {
        ESP_LOGW(TAG, "人脸模块初始化失败，后续人脸打卡将失败提示");
    }
#else
    ESP_LOGW(TAG, "已跳过人脸模块初始化 (ENABLE_FACE_MODULE=0)");
#endif

#if ENABLE_FINGER_MODULE
    ESP_LOGI(TAG, "开始初始化指纹模块...");
    if (app_fingerprint_init() == ESP_OK) {
        s_finger_ready = true;
        ESP_LOGI(TAG, "指纹模块初始化完成");
    } else {
        ESP_LOGW(TAG, "指纹模块未就绪，后续指纹打卡将失败提示");
    }
#else
    ESP_LOGW(TAG, "已跳过指纹模块初始化 (ENABLE_FINGER_MODULE=0)");
#endif

    // app_test_hub_face_enroll_test("zhangsan", false, 10);
    // app_test_hub_face_identify_test(8);

    // app_lvgl_dispatch_event(APP_LVGL_EVT_SELFCHECK_DONE);

    // // 3. 启动传感器应用模块
    // ESP_ERROR_CHECK(app_sensor_start());

    // // 4. 启动网络管理模块 (WiFi + MQTT)
    // ESP_ERROR_CHECK(app_network_start());

    // 5. 启动 RFID 模块（刷卡后驱动 UI 进入身份确认）
#if ENABLE_RFID_MODULE
    ESP_LOGI(TAG, "开始启动 RFID 模块...");
    app_rfid_set_card_callback(on_rfid_card_detected, NULL);
    ESP_ERROR_CHECK(app_rfid_start());
    ESP_LOGI(TAG, "RFID 模块启动完成");
#else
    ESP_LOGW(TAG, "已跳过 RFID 模块初始化 (ENABLE_RFID_MODULE=0)");
#endif

    // // 6. 启动 CO2 采集模块
    // ESP_ERROR_CHECK(app_co2_start());

    ESP_LOGI(TAG, "app_main 执行完毕 (即将在空闲时自行删除)");
    // app_main 函数返回后，其对应的任务会自动删除，但它创建的子任务会继续运行
}
