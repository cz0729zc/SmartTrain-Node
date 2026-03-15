#include "nvs_flash.h"
#include "esp_log.h"
#include "app_wifi.h"
#include "driver_dht.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

// 定义 DHT 传感器配置 (根据实际硬件修改引脚)
#define SENSOR_TYPE DRIVER_DHT_TYPE_DHT11
#define SENSOR_GPIO GPIO_NUM_4  // 示例引脚

// 任务句柄，后续可用于任务间通信或控制
static TaskHandle_t s_dht_task_handle = NULL;
static TaskHandle_t s_network_task_handle = NULL;

void dht_task(void *pvParameters)
{
    float temperature, humidity;
    ESP_LOGI("dht_task", "传感器任务启动...");

    while (1)
    {
        if (driver_dht_read_float_data(SENSOR_TYPE, SENSOR_GPIO, &humidity, &temperature) == ESP_OK) {
            ESP_LOGI("dht_task", "Humidity: %.1f%% Temp: %.1fC", humidity, temperature);
            // TODO: 这里可以将数据发送到队列，供 Network 任务上报
        } else {
            ESP_LOGW("dht_task", "Could not read data from sensor");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void network_task(void *pvParameters)
{
    ESP_LOGI("network_task", "网络任务启动，开始连接 WiFi...");

    // 1. 初始化 WiFi
    app_wifi_init();
    
    // 2. 循环等待连接（这里实现了断网重连的简单逻辑基础）
    // 注意：app_wifi_wait_connected 目前是阻塞的，未来可以改进为事件驱动
    while (1) {
        esp_err_t err = app_wifi_wait_connected();
        if (err == ESP_OK) {
            ESP_LOGI("network_task", "WiFi 已连接，准备启动 MQTT...");
            
            // TODO: 在这里调用 app_mqtt_start()
            // app_mqtt_start();

            // 保持任务运行，用于处理后续的网络维护或等待断开事件
            // 如果连接断开，app_wifi 组件会自动重连，这里我们只需简单的延时检测或等待信号量
            vTaskDelay(pdMS_TO_TICKS(5000)); 
        } else {
            ESP_LOGE("network_task", "WiFi 连接失败，稍后重试...");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
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

    // 2. 创建传感器任务 (独立运行，不受网络影响)
    xTaskCreate(dht_task, "dht_task", 2048, NULL, 5, &s_dht_task_handle);

    // 3. 创建网络通信任务 (独立运行，不阻塞主线程)
    xTaskCreate(network_task, "network_task", 4096, NULL, 5, &s_network_task_handle);

    // 4. (可选) 创建 UI 交互任务
    // xTaskCreate(ui_task, "ui_task", ...);

    ESP_LOGI(TAG, "app_main 执行完毕 (即将在空闲时自行删除)");
    // app_main 函数返回后，其对应的任务会自动删除，但它创建的子任务会继续运行
}
