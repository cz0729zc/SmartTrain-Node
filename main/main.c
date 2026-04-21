#include "nvs_flash.h"
#include "esp_log.h"
#include "sensor_data.h"
#include "app_sensor.h"
#include "app_network.h"
#include "app_rfid.h"
#include "app_co2.h"
#include "app_display.h"
#include "app_lvgl.h"
#include "app_fingerprint.h"
#include "app_face.h"
#include "attendance_profile.h"
#include <string.h>
#include <ctype.h>
#include "app_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* 触摸联调时建议只启用一种显示/输入路径，避免并发读取触摸设备 */
#define RUN_RAW_TOUCH_TEST 0
#define RUN_SCREEN_ONLY_TEST 1
#define RUN_FM225_SELF_TEST 0
#define RUN_AS608_SELF_TEST 0
#define RUN_AS608_ENROLL_DEMO 0
#define RUN_AS608_IDENTIFY_DEMO 0
#define RUN_ATTENDANCE_BIO_FLOW 0
#define AS608_ENROLL_PAGE_ID 0

#define FM225_UART_NUM      UART_NUM_1
#define FM225_UART_TX_GPIO  GPIO_NUM_11
#define FM225_UART_RX_GPIO  GPIO_NUM_10
#define FM225_UART_BAUD     115200

#define RFID_STUDENT_ID_BLOCK 4
#define FACE_VERIFY_TIMEOUT_S 5
#define FACE_ENROLL_TIMEOUT_S 15
#define FINGER_SEARCH_PAGE_MAX 100
#define UI_METHOD_SELECT_TIMEOUT_MS 15000

static const char *TAG = "main";
static SemaphoreHandle_t s_bio_flow_mutex = NULL;
static bool s_fp_ready = false;
static bool s_fm225_ready = false;
static bool s_lvgl_ready = false;

static void ui_show_result_countdown_then_idle(bool success, const char *line1)
{
    if (!s_lvgl_ready) {
        return;
    }

    char countdown[48];
    for (int sec = 2; sec >= 1; --sec) {
        snprintf(countdown, sizeof(countdown), "%d 秒后返回首页", sec);
        app_lvgl_show_result(success, line1, countdown);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    app_lvgl_show_idle("考勤就绪 ATTENDANCE READY",
                       "请刷 RFID 卡  Tap your RFID card",
                       "人脸或指纹  Face or Fingerprint");
}

#if RUN_AS608_ENROLL_DEMO || RUN_AS608_IDENTIFY_DEMO
static void log_fingerprint_result(const char *phase, esp_err_t ret, uint8_t confirm)
{
    if (ret == ESP_OK && confirm == DRIVER_AS608_CONFIRM_OK) {
        ESP_LOGI(TAG, "%s 成功", phase);
        return;
    }

    ESP_LOGW(TAG, "%s 失败: ret=%s, confirm=0x%02X(%s)",
             phase,
             esp_err_to_name(ret),
             confirm,
             app_fingerprint_confirm_message(confirm));
}
#endif

#if RUN_SCREEN_ONLY_TEST
static void ui_showcase_task(void *arg)
{
    (void)arg;

    esp_err_t ret = app_lvgl_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "屏幕单测模式下 LVGL 初始化失败: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    s_lvgl_ready = true;

    while (true) {
        app_lvgl_show_idle("考勤就绪 ATTENDANCE READY",
                           "请刷 RFID 卡  Tap your RFID card",
                           "人脸或指纹  Face or Fingerprint");
        vTaskDelay(pdMS_TO_TICKS(2200));

        app_lvgl_show_method_select("2026001", "张三 ZHANG SAN");
        vTaskDelay(pdMS_TO_TICKS(2600));

        app_lvgl_show_processing("人脸核验 Face Check", "请注视摄像头");
        vTaskDelay(pdMS_TO_TICKS(1800));

        ui_show_result_countdown_then_idle(true, "人脸核验通过 2026001");

        app_lvgl_show_method_select("2026002", "李四 LI SI");
        vTaskDelay(pdMS_TO_TICKS(2400));

        app_lvgl_show_processing("指纹核验 Fingerprint", "请按压指纹传感器");
        vTaskDelay(pdMS_TO_TICKS(1800));

        ui_show_result_countdown_then_idle(false, "指纹核验失败 请重试");
    }
}
#endif

/**
 * @brief 打印块数据 (16 字节)
 */
static void print_block_data(const char *label, const uint8_t *data, size_t len)
{
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

/**
 * @brief RFID 读写测试 (使用单次认证会话)
 */
__attribute__((unused)) static void rfid_read_write_test(void)
{
#if RUN_RAW_TOUCH_TEST
    ESP_ERROR_CHECK(app_display_init());
    app_display_test();  // 原始触摸测试：串口打印触摸坐标
#else
    ESP_ERROR_CHECK(app_lvgl_init());
    app_lvgl_demo();     // LVGL 触摸测试：按钮/界面交互
#endif
    const uint8_t test_block = 4;  // 使用块 4 (Sector 1 的第一个数据块，安全)
    uint8_t read_before[16] = {0};
    uint8_t read_after[16] = {0};
    uint8_t write_buf[16] = {0};
    esp_err_t ret;

    ESP_LOGI(TAG, "========== RFID 读写测试开始 ==========");

    // 准备测试数据 (写入 "Hello RC522!" + 递增计数)
    static uint8_t write_count = 0;
    snprintf((char *)write_buf, sizeof(write_buf), "Hello RC522!%03d", write_count++);
    ESP_LOGI(TAG, "测试数据: \"%s\"", (char *)write_buf);

    // 单次认证完成: 读取原始 → 写入 → 读取验证
    ret = app_rfid_read_write_verify(test_block, write_buf, read_before, read_after);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读写测试失败: %s", esp_err_to_name(ret));
        return;
    }

    // 打印结果
    print_block_data("写入前", read_before, sizeof(read_before));
    print_block_data("写入后", read_after, sizeof(read_after));

    // 验证数据一致性
    if (memcmp(write_buf, read_after, sizeof(write_buf)) == 0) {
        ESP_LOGI(TAG, "[OK] 数据验证成功!");
    } else {
        ESP_LOGE(TAG, "[FAIL] 数据验证失败!");
    }

    ESP_LOGI(TAG, "========== RFID 读写测试结束 ==========");
}

/**
 * @brief RFID 卡片检测回调
 */
static void on_rfid_card_detected(const rfid_card_info_t *card, void *arg)
{
    (void)arg;
    attendance_profile_t profile = {0};
    bool found = false;
    bool from_student_id = false;

    if (card == NULL) {
        return;
    }

    if (s_bio_flow_mutex != NULL) {
        if (xSemaphoreTake(s_bio_flow_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
            ESP_LOGW(TAG, "生物验证流程忙，忽略本次刷卡 UID=%s", card->uid_str);
            return;
        }
    }

    ESP_LOGI(TAG, "检测到 RFID 卡片, UID: %s", card->uid_str);

#if RUN_ATTENDANCE_BIO_FLOW
    if (!s_fm225_ready || !s_fp_ready) {
        ESP_LOGW(TAG, "生物模块未就绪: face=%d, finger=%d，跳过验证", s_fm225_ready, s_fp_ready);
        if (s_lvgl_ready) {
            ui_show_result_countdown_then_idle(false, "生物模块未就绪");
        }
        goto done;
    }

    {
        uint8_t block_data[16] = {0};
        char student_id_on_card[ATTENDANCE_STUDENT_ID_MAX_LEN] = {0};
        esp_err_t ret = app_rfid_read_block(RFID_STUDENT_ID_BLOCK, block_data, sizeof(block_data));
        if (ret == ESP_OK) {
            size_t out = 0;
            for (size_t i = 0; i < sizeof(block_data) && out < sizeof(student_id_on_card) - 1; ++i) {
                uint8_t ch = block_data[i];
                if (ch == 0x00 || ch == 0xFF) {
                    break;
                }
                if (isprint(ch)) {
                    student_id_on_card[out++] = (char)ch;
                }
            }

            if (student_id_on_card[0] != '\0' &&
                attendance_profile_find_by_student_id(student_id_on_card, &profile) == ESP_OK) {
                found = true;
                from_student_id = true;
                ESP_LOGI(TAG, "卡片学号=%s，档案命中", student_id_on_card);
            }
        } else {
            ESP_LOGW(TAG, "读取卡片学号块失败: %s", esp_err_to_name(ret));
        }
    }
#endif

    if (!found && attendance_profile_find_by_uid(card->uid_str, &profile) == ESP_OK) {
        found = true;
        ESP_LOGI(TAG, "通过 UID 命中档案: 学号=%s, 姓名=%s", profile.student_id, profile.name);
    }

    if (!found) {
        ESP_LOGW(TAG, "未注册卡片: UID=%s", card->uid_str);
        if (s_lvgl_ready) {
            ui_show_result_countdown_then_idle(false, "未注册卡片 请先注册");
        }
        goto done;
    }

    if (from_student_id && strcmp(profile.uid, card->uid_str) != 0) {
        ESP_LOGW(TAG, "学号命中但 UID 不一致: card=%s, db=%s", card->uid_str, profile.uid);
    }

    ESP_LOGI(TAG, "识别用户: 学号=%s, 姓名=%s", profile.student_id, profile.name);

#if RUN_ATTENDANCE_BIO_FLOW
    if (!profile.has_face_bound || !profile.has_finger_bound) {
        uint8_t face_result = 0xFF;
        uint8_t finger_confirm = 0xFF;
        uint16_t face_user_id = ATTENDANCE_FACE_ID_UNBOUND;
        uint16_t finger_page_id = ATTENDANCE_FINGER_ID_UNBOUND;

        ESP_LOGI(TAG, "检测到未绑定生物信息，开始注册流程");
        if (s_lvgl_ready) {
            app_lvgl_show_processing("首次注册 Enrollment", "正在录入生物信息...");
        }

        if (!profile.has_face_bound) {
            esp_err_t ret = app_face_enroll_single(profile.student_id,
                                                   false,
                                                   FACE_ENROLL_TIMEOUT_S,
                                                   &face_user_id,
                                                   &face_result);
            if (ret != ESP_OK || face_result != DRIVER_FM225_RESULT_SUCCESS) {
                ESP_LOGW(TAG, "人脸注册失败: ret=%s, result=%u(%s)",
                         esp_err_to_name(ret),
                         (unsigned)face_result,
                         app_face_result_message(face_result));
                goto done;
            }

            if (attendance_profile_bind_face_id(profile.uid, face_user_id) != ESP_OK) {
                ESP_LOGW(TAG, "人脸ID绑定失败: uid=%s, face_id=%u", profile.uid, (unsigned)face_user_id);
                goto done;
            }
            profile.face_user_id = face_user_id;
            profile.has_face_bound = true;
            ESP_LOGI(TAG, "人脸绑定完成: face_id=%u", (unsigned)face_user_id);
        }

        if (!profile.has_finger_bound) {
            attendance_profile_t probe = {0};
            for (uint16_t page = 0; page < FINGER_SEARCH_PAGE_MAX; ++page) {
                if (attendance_profile_find_by_finger_page(page, &probe) == ESP_ERR_NOT_FOUND) {
                    finger_page_id = page;
                    break;
                }
            }
            if (finger_page_id == ATTENDANCE_FINGER_ID_UNBOUND) {
                ESP_LOGW(TAG, "无可用指纹页号，注册中止");
                goto done;
            }

            esp_err_t ret = app_fingerprint_enroll(finger_page_id, &finger_confirm);
            if (ret != ESP_OK || finger_confirm != DRIVER_AS608_CONFIRM_OK) {
                ESP_LOGW(TAG, "指纹注册失败: ret=%s, confirm=0x%02X(%s)",
                         esp_err_to_name(ret),
                         finger_confirm,
                         app_fingerprint_confirm_message(finger_confirm));
                goto done;
            }

            if (attendance_profile_bind_finger_page(profile.uid, finger_page_id) != ESP_OK) {
                ESP_LOGW(TAG, "指纹页号绑定失败: uid=%s, page=%u", profile.uid, (unsigned)finger_page_id);
                goto done;
            }
            profile.finger_page_id = finger_page_id;
            profile.has_finger_bound = true;
            ESP_LOGI(TAG, "指纹绑定完成: page_id=%u", (unsigned)finger_page_id);
        }

        ESP_LOGI(TAG, "首次生物注册完成: 学号=%s", profile.student_id);
        if (s_lvgl_ready) {
            ui_show_result_countdown_then_idle(true, "注册完成 下次可二选一打卡");
        }
        goto done;
    }

    {
        app_lvgl_checkin_method_t selected = APP_LVGL_CHECKIN_METHOD_FACE;
        uint8_t face_result = 0xFF;
        uint8_t finger_confirm = 0xFF;
        driver_fm225_verify_result_t face_verify = {0};
        driver_as608_search_result_t finger_verify = {0};

        if (s_lvgl_ready) {
            app_lvgl_show_method_select(profile.student_id, profile.name);
            esp_err_t wait_ret = app_lvgl_wait_method_select(UI_METHOD_SELECT_TIMEOUT_MS, &selected);
            if (wait_ret != ESP_OK) {
                ESP_LOGW(TAG, "等待打卡方式选择超时/失败: %s", esp_err_to_name(wait_ret));
                ui_show_result_countdown_then_idle(false, "选择超时 请重新刷卡");
                goto done;
            }
        }

        if (selected == APP_LVGL_CHECKIN_METHOD_FACE) {
            if (s_lvgl_ready) {
                app_lvgl_show_processing("人脸核验 Face Check", "请注视摄像头");
            }

            esp_err_t ret = app_face_verify_once(FACE_VERIFY_TIMEOUT_S, &face_verify, &face_result);
            if (ret != ESP_OK || face_result != DRIVER_FM225_RESULT_SUCCESS) {
                ESP_LOGW(TAG, "人脸验证失败: ret=%s, result=%u(%s)",
                         esp_err_to_name(ret),
                         (unsigned)face_result,
                         app_face_result_message(face_result));
                if (s_lvgl_ready) {
                    ui_show_result_countdown_then_idle(false, "人脸核验失败 请重试");
                }
                goto done;
            }

            if (face_verify.user_id != profile.face_user_id) {
                ESP_LOGW(TAG, "人脸ID不匹配: expected=%u, got=%u",
                         (unsigned)profile.face_user_id,
                         (unsigned)face_verify.user_id);
                if (s_lvgl_ready) {
                    ui_show_result_countdown_then_idle(false, "人脸ID不匹配");
                }
                goto done;
            }

            ESP_LOGI(TAG, "打卡验证通过(人脸): 学号=%s, 姓名=%s", profile.student_id, profile.name);
            if (s_lvgl_ready) {
                char msg[64] = {0};
                snprintf(msg, sizeof(msg), "人脸核验通过 学号:%s", profile.student_id);
                ui_show_result_countdown_then_idle(true, msg);
            }
            goto done;
        }

        if (s_lvgl_ready) {
            app_lvgl_show_processing("指纹核验 Fingerprint", "请按压指纹传感器");
        }

        esp_err_t ret = app_fingerprint_identify(&finger_verify, &finger_confirm);
        if (ret != ESP_OK || finger_confirm != DRIVER_AS608_CONFIRM_OK) {
            ESP_LOGW(TAG, "指纹验证失败: ret=%s, confirm=0x%02X(%s)",
                     esp_err_to_name(ret),
                     finger_confirm,
                     app_fingerprint_confirm_message(finger_confirm));
            if (s_lvgl_ready) {
                ui_show_result_countdown_then_idle(false, "指纹核验失败 请重试");
            }
            goto done;
        }

        if (finger_verify.page_id != profile.finger_page_id) {
            ESP_LOGW(TAG, "指纹页号不匹配: expected=%u, got=%u, score=%u",
                     (unsigned)profile.finger_page_id,
                     (unsigned)finger_verify.page_id,
                     (unsigned)finger_verify.match_score);
            if (s_lvgl_ready) {
                ui_show_result_countdown_then_idle(false, "指纹ID不匹配");
            }
            goto done;
        }

        ESP_LOGI(TAG, "打卡验证通过(指纹): 学号=%s, 姓名=%s, 指纹分数=%u",
                 profile.student_id,
                 profile.name,
                 (unsigned)finger_verify.match_score);
        if (s_lvgl_ready) {
            char msg[64] = {0};
            snprintf(msg, sizeof(msg), "指纹核验通过 学号:%s", profile.student_id);
            ui_show_result_countdown_then_idle(true, msg);
        }
    }
#endif

    // 调试用读写测试（默认关闭，避免覆盖业务数据）
    // rfid_read_write_test();

done:
    if (s_bio_flow_mutex != NULL) {
        xSemaphoreGive(s_bio_flow_mutex);
    }
}

static bool fingerprint_self_test(void)
{
    esp_err_t ret;
    uint8_t confirm = 0xFF;
    driver_as608_sys_para_t para = {0};

    ret = app_fingerprint_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AS608 初始化失败: %s", esp_err_to_name(ret));
        return false;
    }

    for (int i = 0; i < 3; ++i) {
        ret = app_fingerprint_get_sys_para(&para, &confirm);
        if (ret == ESP_OK && confirm == DRIVER_AS608_CONFIRM_OK) {
            ESP_LOGI(TAG, "AS608 参数: 容量=%u, 安全等级=%u, 地址=0x%08lX, 波特率=%lu",
                     para.max_templates,
                     para.security_level,
                     (unsigned long)para.device_addr,
                     (unsigned long)(para.baud_factor_n * 9600U));
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ESP_LOGW(TAG, "读取 AS608 参数失败: ret=%s, confirm=0x%02X(%s)",
             esp_err_to_name(ret), confirm, app_fingerprint_confirm_message(confirm));
    ESP_LOGW(TAG, "AS608 自检未通过，已跳过注册/识别演示任务。请优先检查: 1) 模块 TX->GPIO16, RX->GPIO17; 2) 5V/3.3V 供电与 GND 共地; 3) 模块串口波特率是否为 57600");
    return false;
}

static const char *fm225_status_message(driver_fm225_status_t status)
{
    switch (status) {
        case DRIVER_FM225_STATUS_IDLE: return "IDLE";
        case DRIVER_FM225_STATUS_BUSY: return "BUSY";
        case DRIVER_FM225_STATUS_ERROR: return "ERROR";
        case DRIVER_FM225_STATUS_INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}

static bool fm225_self_test(void)
{
    app_face_config_t cfg = {
        .uart_num = FM225_UART_NUM,
        .tx_gpio = FM225_UART_TX_GPIO,
        .rx_gpio = FM225_UART_RX_GPIO,
        .baud_rate = FM225_UART_BAUD,
        .cmd_timeout_ms = 3000,
    };

    esp_err_t ret = app_face_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FM225 初始化失败: %s", esp_err_to_name(ret));
        return false;
    }

    ret = app_face_wait_ready(5000);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FM225 等待 READY 超时/失败: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "请检查连线: 模组 TX -> GPIO10, 模组 RX -> GPIO11, GND 共地, 波特率 115200");
        return false;
    }

    driver_fm225_status_t status = DRIVER_FM225_STATUS_INVALID;
    uint8_t result_code = 0xFF;
    ret = app_face_get_status(&status, &result_code);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FM225 GET_STATUS 通信失败: %s", esp_err_to_name(ret));
        return false;
    }

    if (result_code != DRIVER_FM225_RESULT_SUCCESS) {
        ESP_LOGW(TAG, "FM225 GET_STATUS 返回失败: result=%u(%s)",
                 (unsigned)result_code,
                 app_face_result_message(result_code));
        return false;
    }

    ESP_LOGI(TAG, "FM225 自检通过: status=%u(%s)",
             (unsigned)status,
             fm225_status_message(status));
    return true;
}

#if RUN_AS608_ENROLL_DEMO || RUN_AS608_IDENTIFY_DEMO
static void fingerprint_demo_task(void *arg)
{
    (void)arg;

#if RUN_AS608_ENROLL_DEMO
    {
        uint8_t confirm = 0xFF;
        ESP_LOGI(TAG, "[AS608] 开始注册流程, page_id=%u", (unsigned)AS608_ENROLL_PAGE_ID);
        ESP_LOGI(TAG, "[AS608] 请同一手指按压两次传感器进行建模");
        esp_err_t ret = app_fingerprint_enroll(AS608_ENROLL_PAGE_ID, &confirm);
        log_fingerprint_result("注册流程", ret, confirm);
    }
#endif

#if RUN_AS608_IDENTIFY_DEMO
    ESP_LOGI(TAG, "[AS608] 进入识别循环, 请按压已录入手指");
    while (true) {
        uint8_t confirm = 0xFF;
        driver_as608_search_result_t result = {0};
        esp_err_t ret = app_fingerprint_identify(&result, &confirm);

        if (ret == ESP_OK && confirm == DRIVER_AS608_CONFIRM_OK) {
            ESP_LOGI(TAG, "识别成功: page_id=%u, score=%u",
                     (unsigned)result.page_id,
                     (unsigned)result.match_score);
        } else if (ret == ESP_OK && confirm == DRIVER_AS608_CONFIRM_NO_FINGER) {
            // 无手指时静默等待，避免刷屏。
        } else {
            log_fingerprint_result("识别流程", ret, confirm);
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
#else
    vTaskDelete(NULL);
#endif
}
#endif

void app_main(void)
{
    bool boot_screen_ready = false;

    // 1. 系统级初始化 (NVS, Log, BSP...)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if RUN_SCREEN_ONLY_TEST
#if RUN_RAW_TOUCH_TEST
    ESP_ERROR_CHECK(app_display_init());
    app_display_test();
#else
    xTaskCreate(ui_showcase_task, "ui_showcase", 8192, NULL, 5, NULL);
#endif
    ESP_LOGI(TAG, "当前为屏幕单测模式，正在轮播 UI 设计页面");
    return;
#endif

    if (app_display_boot_init() == ESP_OK) {
        boot_screen_ready = true;
        app_display_boot_show_title("SYSTEM INIT");
        app_display_boot_show_line(0, "NVS: OK", APP_DISPLAY_COLOR_GREEN);
        app_display_boot_show_line(1, "FACE: CHECKING", APP_DISPLAY_COLOR_YELLOW);
        app_display_boot_show_line(2, "FINGER: CHECKING", APP_DISPLAY_COLOR_YELLOW);
        app_display_boot_show_line(3, "RFID: WAIT", APP_DISPLAY_COLOR_YELLOW);
        app_display_boot_show_line(4, "READY: WAIT", APP_DISPLAY_COLOR_YELLOW);
    }

    // 1.1 初始化本地学员档案映射 (UID -> 学号/姓名)    
    ESP_ERROR_CHECK(attendance_profile_init());

    s_bio_flow_mutex = xSemaphoreCreateMutex();
    if (s_bio_flow_mutex == NULL) {
        ESP_LOGE(TAG, "创建生物流程互斥锁失败");
        return;
    }

    // 示例: 首次可手动写入测试档案，确认刷卡后能识别姓名
    // ESP_ERROR_CHECK(attendance_profile_upsert("04:A1:B2:C3:D4", "2026001", "张三"));

    ESP_LOGI(TAG, "系统初始化完成，开始创建并发任务...");

#if RUN_FM225_SELF_TEST
    s_fm225_ready = fm225_self_test();
#else
    s_fm225_ready = true;
#endif

    if (boot_screen_ready) {
        app_display_boot_show_line(1,
                                   s_fm225_ready ? "FACE: OK" : "FACE: FAIL",
                                   s_fm225_ready ? APP_DISPLAY_COLOR_GREEN : APP_DISPLAY_COLOR_RED);
    }

#if RUN_AS608_SELF_TEST
    s_fp_ready = fingerprint_self_test();
#else
    s_fp_ready = true;
#endif

    if (boot_screen_ready) {
        app_display_boot_show_line(2,
                                   s_fp_ready ? "FINGER: OK" : "FINGER: FAIL",
                                   s_fp_ready ? APP_DISPLAY_COLOR_GREEN : APP_DISPLAY_COLOR_RED);
    }

#if RUN_FM225_SELF_TEST
    if (!s_fm225_ready) {
        ESP_LOGW(TAG, "FM225 自检未通过，当前仅保留基础系统流程");
    }
#endif

#if RUN_AS608_ENROLL_DEMO || RUN_AS608_IDENTIFY_DEMO
    if (s_fp_ready) {
        xTaskCreate(fingerprint_demo_task, "fp_demo", 4096, NULL, 5, NULL);
    }
#endif

    // 2. 初始化传感器数据队列 (用于 sensor_task 与 network_task 通信)
    ESP_ERROR_CHECK(sensor_queue_init(5));

    // 3. 初始化 LVGL 显示模块 (LCD + 触摸屏)
    if (app_lvgl_init() == ESP_OK) {
        s_lvgl_ready = true;
        app_lvgl_show_idle("考勤就绪 ATTENDANCE READY",
                           "请刷 RFID 卡  Tap your RFID card",
                           "人脸或指纹  Face or Fingerprint");
    } else {
        ESP_LOGW(TAG, "LVGL 初始化失败，继续使用串口日志流程");
    }

    // // 3. 启动传感器应用模块
    // ESP_ERROR_CHECK(app_sensor_start());

    // // 4. 启动网络管理模块 (WiFi + MQTT)
    // ESP_ERROR_CHECK(app_network_start());

    // 5. 启动 RFID 模块
    if (boot_screen_ready) {
        app_display_boot_show_line(3, "RFID: STARTING", APP_DISPLAY_COLOR_YELLOW);
    }
    app_rfid_set_card_callback(on_rfid_card_detected, NULL);
    ESP_ERROR_CHECK(app_rfid_start());
    if (boot_screen_ready) {
        app_display_boot_show_line(3, "RFID: OK", APP_DISPLAY_COLOR_GREEN);
        app_display_boot_show_line(4, "READY: TAP CARD", APP_DISPLAY_COLOR_GREEN);
    }

    // // 6. 启动 CO2 采集模块
    // ESP_ERROR_CHECK(app_co2_start());

    ESP_LOGI(TAG, "app_main 执行完毕 (即将在空闲时自行删除)");
    // app_main 函数返回后，其对应的任务会自动删除，但它创建的子任务会继续运行
}
