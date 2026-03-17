#include "nvs_flash.h"
#include "esp_log.h"
#include "app_sensor.h"
#include "app_network.h"

static const char *TAG = "main";

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

    // 2. 启动传感器应用模块
    ESP_ERROR_CHECK(app_sensor_start());

    // 3. 启动网络管理模块 (WiFi + MQTT)
    // ESP_ERROR_CHECK(app_network_start());

    // 4. (可选) 创建 UI 交互任务
    // xTaskCreate(ui_task, "ui_task", ...);

    ESP_LOGI(TAG, "app_main 执行完毕 (即将在空闲时自行删除)");
    // app_main 函数返回后，其对应的任务会自动删除，但它创建的子任务会继续运行
}
