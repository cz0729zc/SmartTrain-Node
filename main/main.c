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

static const char *TAG = "main";

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
    attendance_profile_t profile = {0};

    ESP_LOGI(TAG, "检测到 RFID 卡片, UID: %s", card->uid_str);

    if (attendance_profile_find_by_uid(card->uid_str, &profile) == ESP_OK) {
        ESP_LOGI(TAG, "识别成功: 学号=%s, 姓名=%s", profile.student_id, profile.name);
    } else {
        ESP_LOGW(TAG, "未注册卡片: UID=%s", card->uid_str);
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

    // 集中执行可选自检与 demo 任务（由 app_test_hub.c 的 RUN_* 宏控制）
    // app_test_hub_run_startup_tests();
    // app_test_hub_start_optional_tasks();

    // 2. 初始化传感器数据队列 (用于 sensor_task 与 network_task 通信)
    // ESP_ERROR_CHECK(sensor_queue_init(5));

    // 3. 初始化 LVGL 显示模块 (LCD + 触摸屏)
    ESP_ERROR_CHECK(app_lvgl_init());
    app_lvgl_demo();  // 显示测试界面

    // // 3. 启动传感器应用模块
    // ESP_ERROR_CHECK(app_sensor_start());

    // // 4. 启动网络管理模块 (WiFi + MQTT)
    // ESP_ERROR_CHECK(app_network_start());

    // // 5. 启动 RFID 模块（启用后会触发 on_rfid_card_detected 回调）
    // app_rfid_set_card_callback(on_rfid_card_detected, NULL);
    // ESP_ERROR_CHECK(app_rfid_start());

    // // 6. 启动 CO2 采集模块
    // ESP_ERROR_CHECK(app_co2_start());

    ESP_LOGI(TAG, "app_main 执行完毕 (即将在空闲时自行删除)");
    // app_main 函数返回后，其对应的任务会自动删除，但它创建的子任务会继续运行
}
