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
#include "app_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 触摸联调时建议只启用一种显示/输入路径，避免并发读取触摸设备 */
#define RUN_RAW_TOUCH_TEST 1
#define RUN_FM225_SELF_TEST 1
#define RUN_AS608_SELF_TEST 1
#define RUN_AS608_ENROLL_DEMO 1
#define RUN_AS608_IDENTIFY_DEMO 1
#define AS608_ENROLL_PAGE_ID 0

#define FM225_UART_NUM      UART_NUM_1
#define FM225_UART_TX_GPIO  GPIO_NUM_11
#define FM225_UART_RX_GPIO  GPIO_NUM_10
#define FM225_UART_BAUD     115200

static const char *TAG = "main";

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
static void rfid_read_write_test(void)
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
    attendance_profile_t profile = {0};

    ESP_LOGI(TAG, "检测到 RFID 卡片, UID: %s", card->uid_str);

    if (attendance_profile_find_by_uid(card->uid_str, &profile) == ESP_OK) {
        ESP_LOGI(TAG, "识别成功: 学号=%s, 姓名=%s", profile.student_id, profile.name);
    } else {
        ESP_LOGW(TAG, "未注册卡片: UID=%s", card->uid_str);
    }

    // 调试用读写测试（默认关闭，避免覆盖业务数据）
    // rfid_read_write_test();
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

void app_main(void)
{
    bool fp_ready = true;
    bool fm225_ready = true;

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

#if RUN_FM225_SELF_TEST
    fm225_ready = fm225_self_test();
#endif

#if RUN_AS608_SELF_TEST
    fp_ready = fingerprint_self_test();
#endif

#if RUN_FM225_SELF_TEST
    if (!fm225_ready) {
        ESP_LOGW(TAG, "FM225 自检未通过，当前仅保留基础系统流程");
    }
#endif

#if RUN_AS608_ENROLL_DEMO || RUN_AS608_IDENTIFY_DEMO
    if (fp_ready) {
        xTaskCreate(fingerprint_demo_task, "fp_demo", 4096, NULL, 5, NULL);
    }
#endif

    // 2. 初始化传感器数据队列 (用于 sensor_task 与 network_task 通信)
    ESP_ERROR_CHECK(sensor_queue_init(5));

    // 3. 初始化 LVGL 显示模块 (LCD + 触摸屏)
    // ESP_ERROR_CHECK(app_lvgl_init());
    // app_lvgl_demo();  // 显示测试界面

    // // 3. 启动传感器应用模块
    // ESP_ERROR_CHECK(app_sensor_start());

    // // 4. 启动网络管理模块 (WiFi + MQTT)
    // ESP_ERROR_CHECK(app_network_start());

    // // 5. 启动 RFID 模块
    // app_rfid_set_card_callback(on_rfid_card_detected, NULL);
    // ESP_ERROR_CHECK(app_rfid_start());

    // // 6. 启动 CO2 采集模块
    // ESP_ERROR_CHECK(app_co2_start());

    ESP_LOGI(TAG, "app_main 执行完毕 (即将在空闲时自行删除)");
    // app_main 函数返回后，其对应的任务会自动删除，但它创建的子任务会继续运行
}
