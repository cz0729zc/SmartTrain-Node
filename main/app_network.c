#include "app_network.h"
#include "app_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "app_network";

static TaskHandle_t s_network_task_handle = NULL;

static void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "网络任务启动，开始连接 WiFi...");

    // 1. 初始化 WiFi 组件 (调用 components/app_wifi)
    app_wifi_init();
    
    // 2. 循环等待连接（这里实现了断网重连的简单逻辑基础）
    while (1) {
        // 阻塞等待连接成功
        esp_err_t err = app_wifi_wait_connected();
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "WiFi 已连接，准备启动 MQTT...");
            
            // TODO: 在这里启动 MQTT 客户端
            // app_mqtt_start();

            // 保持任务运行，用于处理后续的网络维护或等待断开事件
            // 如果连接断开，app_wifi 组件会自动重连，这里我们只需简单的延时检测或等待信号量
            // 后续可以使用 EventGroup 等待 WIFI_FAIL 或 MQTT_DISCONNECT 事件
            vTaskDelay(pdMS_TO_TICKS(5000)); 
        } else {
            ESP_LOGE(TAG, "WiFi 连接失败，稍后重试...");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

esp_err_t app_network_start(void)
{
    if (s_network_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE; // 任务已存在
    }

    // 创建网络任务，栈大小分配 4096 字节，因为后续 MQTT 和 TLS 需要较大栈空间
    BaseType_t ret = xTaskCreate(network_task, "network_task", 4096, NULL, 5, &s_network_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建网络任务失败");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
