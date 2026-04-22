#include "app_test_hub.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_display.h"
#include "app_face.h"
#include "app_fingerprint.h"
#include "app_lvgl.h"
#include "app_rfid.h"
#include <string.h>

/*
 * 统一测试开关：所有 test/demo 入口只在本文件维护。
 * 推荐用法：
 * - 日常业务联调：全部设为 0
 * - 单模块自检：只打开对应 RUN_* 宏
 */
#define RUN_RAW_TOUCH_TEST 0
#define RUN_FM225_SELF_TEST 1
#define RUN_AS608_SELF_TEST 0
#define RUN_AS608_ENROLL_DEMO 0
#define RUN_AS608_IDENTIFY_DEMO 0
#define AS608_ENROLL_PAGE_ID 0

#define FM225_UART_NUM      UART_NUM_1
#define FM225_UART_TX_GPIO  GPIO_NUM_11
#define FM225_UART_RX_GPIO  GPIO_NUM_10
#define FM225_UART_BAUD     115200

static const char *TAG = "app_test_hub";
#if RUN_AS608_SELF_TEST || RUN_AS608_ENROLL_DEMO || RUN_AS608_IDENTIFY_DEMO
static bool s_fp_ready = true;
#endif

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

static void print_block_data(const char *label, const uint8_t *data, size_t len)
{
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

static esp_err_t ensure_fm225_link(uint32_t ready_timeout_ms)
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
        return ret;
    }

    ret = app_face_wait_ready(ready_timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FM225 READY 未收到: %s (继续尝试 GET_STATUS)", esp_err_to_name(ret));
    }

    driver_fm225_status_t status = DRIVER_FM225_STATUS_INVALID;
    uint8_t result_code = 0xFF;
    ret = app_face_get_status(&status, &result_code);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FM225 GET_STATUS 通信失败: %s", esp_err_to_name(ret));
        return ret;
    }

    if (result_code != DRIVER_FM225_RESULT_SUCCESS) {
        ESP_LOGE(TAG, "FM225 GET_STATUS 失败: result=%u(%s)",
                 (unsigned)result_code,
                 app_face_result_message(result_code));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "FM225 链路可用，status=%u", (unsigned)status);
    return ESP_OK;
}

void app_test_hub_rfid_read_write_test(void)
{
#if RUN_RAW_TOUCH_TEST
    // 原始触摸链路调试：更贴近底层，便于排查触摸驱动问题。
#else
    // LVGL 页面调试：验证图形层与触摸交互是否正常。
#endif
#if RUN_RAW_TOUCH_TEST
    ESP_ERROR_CHECK(app_display_init());
    app_display_test();
#else
    ESP_ERROR_CHECK(app_lvgl_init());
    app_lvgl_demo();
#endif

    const uint8_t test_block = 4;
    uint8_t read_before[16] = {0};
    uint8_t read_after[16] = {0};
    uint8_t write_buf[16] = {0};
    esp_err_t ret;

    ESP_LOGI(TAG, "========== RFID 读写测试开始 ==========");

    static uint8_t write_count = 0;
    snprintf((char *)write_buf, sizeof(write_buf), "Hello RC522!%03d", write_count++);
    ESP_LOGI(TAG, "测试数据: \"%s\"", (char *)write_buf);

    ret = app_rfid_read_write_verify(test_block, write_buf, read_before, read_after);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读写测试失败: %s", esp_err_to_name(ret));
        return;
    }

    print_block_data("写入前", read_before, sizeof(read_before));
    print_block_data("写入后", read_after, sizeof(read_after));

    if (memcmp(write_buf, read_after, sizeof(write_buf)) == 0) {
        ESP_LOGI(TAG, "[OK] 数据验证成功!");
    } else {
        ESP_LOGE(TAG, "[FAIL] 数据验证失败!");
    }

    ESP_LOGI(TAG, "========== RFID 读写测试结束 ==========");
}

void app_test_hub_face_enroll_test(const char *user_name, bool admin, uint8_t timeout_s)
{
    if (timeout_s == 0) {
        timeout_s = 8;
    }

    const char *name = (user_name != NULL && user_name[0] != '\0') ? user_name : "TEST_USER";

    esp_err_t ret = ensure_fm225_link(5000);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "跳过人脸注册测试：FM225 链路不可用");
        return;
    }

    uint16_t user_id = 0;
    uint8_t result_code = 0xFF;

    ESP_LOGI(TAG, "[FM225] 开始注册测试: name=%s admin=%u timeout=%us",
             name,
             (unsigned)(admin ? 1U : 0U),
             (unsigned)timeout_s);

    ret = app_face_enroll_single(name, admin, timeout_s, &user_id, &result_code);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[FM225] 注册通信失败: %s", esp_err_to_name(ret));
        return;
    }

    if (result_code == DRIVER_FM225_RESULT_SUCCESS) {
        ESP_LOGI(TAG, "[FM225] 注册成功: user_id=%u", (unsigned)user_id);
    } else {
        ESP_LOGW(TAG, "[FM225] 注册失败: result=%u(%s)",
                 (unsigned)result_code,
                 app_face_result_message(result_code));
    }
}

void app_test_hub_face_identify_test(uint8_t timeout_s)
{
    if (timeout_s == 0) {
        timeout_s = 8;
    }

    esp_err_t ret = ensure_fm225_link(5000);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "跳过人脸识别测试：FM225 链路不可用");
        return;
    }

    driver_fm225_verify_result_t verify = {0};
    uint8_t result_code = 0xFF;

    ESP_LOGI(TAG, "[FM225] 开始识别测试: timeout=%us，请正视模块", (unsigned)timeout_s);

    ret = app_face_verify_once(timeout_s, &verify, &result_code);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[FM225] 识别通信失败: %s", esp_err_to_name(ret));
        return;
    }

    if (result_code == DRIVER_FM225_RESULT_SUCCESS) {
        ESP_LOGI(TAG, "[FM225] 识别成功: user_id=%u name=%s admin=%u unlock=%u",
                 (unsigned)verify.user_id,
                 verify.user_name,
                 (unsigned)verify.admin,
                 (unsigned)verify.unlock_status);
    } else {
        ESP_LOGW(TAG, "[FM225] 识别失败: result=%u(%s)",
                 (unsigned)result_code,
                 app_face_result_message(result_code));
    }
}

#if RUN_AS608_SELF_TEST || RUN_AS608_ENROLL_DEMO || RUN_AS608_IDENTIFY_DEMO
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
    ESP_LOGW(TAG, "AS608 自检未通过，已跳过注册/识别演示任务。请检查接线、电源和波特率");
    return false;
}
#endif

#if RUN_FM225_SELF_TEST
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
        ESP_LOGW(TAG, "继续执行主动探测 GET_STATUS，用于判断链路是否可通信");
    }

    driver_fm225_status_t status = DRIVER_FM225_STATUS_INVALID;
    uint8_t result_code = 0xFF;
    ret = app_face_get_status(&status, &result_code);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FM225 GET_STATUS 通信失败: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "请检查连线: 模组 TX -> GPIO10, 模组 RX -> GPIO11, GND 共地, 波特率 115200");
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
#endif

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

void app_test_hub_run_startup_tests(void)
{
    // 启动阶段自检：只执行一次，不创建常驻任务。
#if RUN_FM225_SELF_TEST
    bool fm225_ready = fm225_self_test();
    if (!fm225_ready) {
        ESP_LOGW(TAG, "FM225 自检未通过，当前仅保留基础系统流程");
    }
#endif

#if RUN_AS608_SELF_TEST
    s_fp_ready = fingerprint_self_test();
#endif

#if (RUN_AS608_ENROLL_DEMO || RUN_AS608_IDENTIFY_DEMO) && !(RUN_AS608_SELF_TEST)
    s_fp_ready = fingerprint_self_test();
#endif
}

void app_test_hub_start_optional_tasks(void)
{
    // 可选演示任务：需要持续轮询时才创建 FreeRTOS 任务。
#if RUN_AS608_ENROLL_DEMO || RUN_AS608_IDENTIFY_DEMO
    if (s_fp_ready) {
        xTaskCreate(fingerprint_demo_task, "fp_demo", 4096, NULL, 5, NULL);
    }
#endif
}
