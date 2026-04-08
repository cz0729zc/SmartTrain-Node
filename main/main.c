#include "nvs_flash.h"
#include "esp_log.h"
#include "sensor_data.h"
#include "app_sensor.h"
#include "app_network.h"
#include "app_rfid.h"
#include "app_co2.h"
#include "app_display.h"
#include <string.h>

static const char *TAG = "main";

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
    ESP_LOGI(TAG, "检测到 RFID 卡片, UID: %s", card->uid_str);

    // 执行读写测试
    rfid_read_write_test();
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

    ESP_LOGI(TAG, "系统初始化完成，开始创建并发任务...");

    // 2. 初始化传感器数据队列 (用于 sensor_task 与 network_task 通信)
    // ESP_ERROR_CHECK(sensor_queue_init(5));

    // 3. 初始化并测试显示模块 (LCD + 触摸屏)
    ESP_ERROR_CHECK(app_display_init());
    app_display_test();  // 注意: 此函数会阻塞，用于测试

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
