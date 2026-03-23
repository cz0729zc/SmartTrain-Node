#include "nvs_flash.h"
#include "esp_log.h"
#include "sensor_data.h"
#include "app_sensor.h"
#include "app_network.h"
#include "app_rfid.h"

static const char *TAG = "main";

/**
 * @brief RFID 卡片检测回调
 */
static void on_rfid_card_detected(const rfid_card_info_t *card, void *arg)
{
    ESP_LOGI(TAG, "检测到 RFID 卡片, UID: %s", card->uid_str);

    // TODO: 在这里添加业务逻辑
    // 例如: 上传卡片信息到 MQTT, 记录打卡等
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
    ESP_ERROR_CHECK(sensor_queue_init(5));

    // 3. 启动传感器应用模块
    ESP_ERROR_CHECK(app_sensor_start());

    // 4. 启动网络管理模块 (WiFi + MQTT)
    ESP_ERROR_CHECK(app_network_start());

    // 5. 启动 RFID 模块
    app_rfid_set_card_callback(on_rfid_card_detected, NULL);
    ESP_ERROR_CHECK(app_rfid_start());

    ESP_LOGI(TAG, "app_main 执行完毕 (即将在空闲时自行删除)");
    // app_main 函数返回后，其对应的任务会自动删除，但它创建的子任务会继续运行
}
