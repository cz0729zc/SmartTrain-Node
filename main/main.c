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

/* 1=启用 7 页面自动轮播测试，0=正常业务流程 */
#define RUN_UI_CYCLE_TEST   0
#define RUN_UI_GUIDER_TEST   1

static bool s_face_ready = false;
static bool s_finger_ready = false;
static bool s_has_active_profile = false;
static attendance_profile_t s_active_profile = {0};
static SemaphoreHandle_t s_bio_mutex = NULL;

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

static void biometric_verify_task(void *arg)
{
    app_lvgl_event_t evt = (app_lvgl_event_t)(intptr_t)arg;
    bool success = false;
    const char *method = "未知";
    const char *reason = "无活动用户";
    attendance_profile_t profile = s_active_profile;

    if (!s_has_active_profile) {
        char tbuf[16] = {0};
        format_time_text(tbuf, sizeof(tbuf));
        app_lvgl_update_result(false, method, reason, tbuf);
        app_lvgl_dispatch_event(APP_LVGL_EVT_TIMEOUT);
        xSemaphoreGive(s_bio_mutex);
        vTaskDelete(NULL);
        return;
    }

    if (evt == APP_LVGL_EVT_SELECT_FACE) {
        method = "人脸";

        if (!s_face_ready) {
            reason = "人脸模块未就绪";
        } else if (!profile.has_face_bound) {
            reason = "档案未绑定人脸";
        } else {
            driver_fm225_verify_result_t verify = {0};
            uint8_t result_code = 0xFF;
            esp_err_t ret = app_face_verify_once(8, &verify, &result_code);

            if (ret == ESP_OK && result_code == DRIVER_FM225_RESULT_SUCCESS) {
                if (verify.user_id == profile.face_user_id) {
                    success = true;
                    reason = "";
                } else {
                    reason = "人脸身份不一致";
                }
            } else if (ret == ESP_OK) {
                reason = app_face_result_message(result_code);
            } else {
                reason = esp_err_to_name(ret);
            }
        }

        char tbuf[16] = {0};
        format_time_text(tbuf, sizeof(tbuf));
        app_lvgl_update_result(success, method, reason, tbuf);
        app_lvgl_dispatch_event(success ? APP_LVGL_EVT_FACE_OK : APP_LVGL_EVT_FACE_FAIL);
    } else if (evt == APP_LVGL_EVT_SELECT_FINGER) {
        method = "指纹";

        if (!s_finger_ready) {
            reason = "指纹模块未就绪";
        } else if (!profile.has_finger_bound) {
            reason = "档案未绑定指纹";
        } else {
            driver_as608_search_result_t result = {0};
            uint8_t confirm = 0xFF;
            esp_err_t ret = app_fingerprint_identify(&result, &confirm);

            if (ret == ESP_OK && confirm == DRIVER_AS608_CONFIRM_OK) {
                if (result.page_id == profile.finger_page_id) {
                    success = true;
                    reason = "";
                } else {
                    reason = "指纹身份不一致";
                }
            } else if (ret == ESP_OK) {
                reason = app_fingerprint_confirm_message(confirm);
            } else {
                reason = esp_err_to_name(ret);
            }
        }

        char tbuf[16] = {0};
        format_time_text(tbuf, sizeof(tbuf));
        app_lvgl_update_result(success, method, reason, tbuf);
        app_lvgl_dispatch_event(success ? APP_LVGL_EVT_FINGER_OK : APP_LVGL_EVT_FINGER_FAIL);
    }

    xSemaphoreGive(s_bio_mutex);
    vTaskDelete(NULL);
}

static void on_ui_action(app_lvgl_event_t event, void *arg)
{
    (void)arg;

    if (event == APP_LVGL_EVT_SELECT_FACE || event == APP_LVGL_EVT_SELECT_FINGER) {
        if (s_bio_mutex == NULL) {
            return;
        }

        if (xSemaphoreTake(s_bio_mutex, 0) != pdTRUE) {
            ESP_LOGW(TAG, "生物识别任务正在执行，忽略重复触发");
            return;
        }

        BaseType_t ok = xTaskCreate(biometric_verify_task,
                                    "bio_verify",
                                    6144,
                                    (void *)(intptr_t)event,
                                    5,
                                    NULL);
        if (ok != pdPASS) {
            xSemaphoreGive(s_bio_mutex);
            ESP_LOGE(TAG, "创建生物识别任务失败");
        }
    } else if (event == APP_LVGL_EVT_HOME || event == APP_LVGL_EVT_CANCEL) {
        s_has_active_profile = false;
    }
}

/*
 * main.c 只负责业务入口编排：
 * - 系统初始化
 * - 业务模块启动顺序
 * - 绑定回调
 * 所有测试/自检逻辑统一收敛到 app_test_hub。
 */

/**
 * @brief RFID 卡片检测回调
 */
static void __attribute__((unused)) on_rfid_card_detected(const rfid_card_info_t *card, void *arg)
{
    (void)arg;

    if (card == NULL) {
        return;
    }

    if (s_bio_mutex != NULL && xSemaphoreTake(s_bio_mutex, 0) != pdTRUE) {
        ESP_LOGW(TAG, "识别流程进行中，忽略本次刷卡: %s", card->uid_str);
        return;
    }
    if (s_bio_mutex != NULL) {
        xSemaphoreGive(s_bio_mutex);
    }

    attendance_profile_t profile = {0};

    ESP_LOGI(TAG, "检测到 RFID 卡片, UID: %s", card->uid_str);

    if (attendance_profile_find_by_uid(card->uid_str, &profile) == ESP_OK) {
        ESP_LOGI(TAG, "识别成功: 学号=%s, 姓名=%s", profile.student_id, profile.name);

        if (!profile.has_face_bound && !profile.has_finger_bound) {
            app_lvgl_dispatch_event(APP_LVGL_EVT_CARD_UNBOUND);
            return;
        }

        s_active_profile = profile;
        s_has_active_profile = true;

        char uid_suffix[8] = {0};
        extract_uid_suffix(card->uid_str, uid_suffix, sizeof(uid_suffix));
        app_lvgl_update_user_info(profile.student_id, profile.name, uid_suffix);
        app_lvgl_dispatch_event(APP_LVGL_EVT_CARD_DETECTED);
    } else {
        ESP_LOGW(TAG, "未注册卡片: UID=%s", card->uid_str);
        s_has_active_profile = false;
        app_lvgl_dispatch_event(APP_LVGL_EVT_CARD_UNREGISTERED);
    }

    // 调试用读写测试（默认关闭，避免覆盖业务数据）
    // app_test_hub_rfid_read_write_test();
}

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
    ESP_ERROR_CHECK(attendance_profile_init());

    // 示例: 首次可手动写入测试档案，确认刷卡后能识别姓名
    // ESP_ERROR_CHECK(attendance_profile_upsert("04:A1:B2:C3:D4", "2026001", "张三"));

    ESP_LOGI(TAG, "系统初始化完成，开始创建并发任务...");

    s_bio_mutex = xSemaphoreCreateMutex();
    if (s_bio_mutex == NULL) {
        ESP_LOGE(TAG, "创建生物识别互斥锁失败");
    }

    // 集中执行可选自检与 demo 任务（由 app_test_hub.c 的 RUN_* 宏控制）
    // app_test_hub_run_startup_tests();
    // app_test_hub_start_optional_tasks();

    // 2. 初始化传感器数据队列 (用于 sensor_task 与 network_task 通信)
    // ESP_ERROR_CHECK(sensor_queue_init(5));

    // 3. 初始化 LVGL 显示模块 (LCD + 触摸屏)
    ESP_ERROR_CHECK(app_lvgl_init());

#if RUN_UI_GUIDER_TEST
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    return;
#endif

    app_lvgl_set_action_callback(on_ui_action, NULL);
    app_lvgl_demo();


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

    if (app_face_init(&face_cfg) == ESP_OK && app_face_wait_ready(5000) == ESP_OK) {
        s_face_ready = true;
    } else {
        ESP_LOGW(TAG, "人脸模块未就绪，后续人脸打卡将失败提示");
    }

    if (app_fingerprint_init() == ESP_OK) {
        s_finger_ready = true;
    } else {
        ESP_LOGW(TAG, "指纹模块未就绪，后续指纹打卡将失败提示");
    }

    app_test_hub_face_enroll_test("zhangsan", false, 10);
    app_test_hub_face_identify_test(8);

    // app_lvgl_dispatch_event(APP_LVGL_EVT_SELFCHECK_DONE);

    // // 3. 启动传感器应用模块
    // ESP_ERROR_CHECK(app_sensor_start());

    // // 4. 启动网络管理模块 (WiFi + MQTT)
    // ESP_ERROR_CHECK(app_network_start());

    // 5. 启动 RFID 模块（刷卡后驱动 UI 进入身份确认）
    // app_rfid_set_card_callback(on_rfid_card_detected, NULL);
    // ESP_ERROR_CHECK(app_rfid_start());

    // // 6. 启动 CO2 采集模块
    // ESP_ERROR_CHECK(app_co2_start());

    ESP_LOGI(TAG, "app_main 执行完毕 (即将在空闲时自行删除)");
    // app_main 函数返回后，其对应的任务会自动删除，但它创建的子任务会继续运行
}
