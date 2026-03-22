#include "app_network.h"
#include "app_wifi.h"
#include "app_mqtt.h"
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

            // 启动 MQTT 客户端
            esp_err_t mqtt_err = app_mqtt_start();
            if (mqtt_err == ESP_OK) {
                ESP_LOGI(TAG, "MQTT 客户端启动成功");
            } else if (mqtt_err == ESP_ERR_INVALID_STATE) {
                ESP_LOGD(TAG, "MQTT 客户端已在运行");
            } else {
                ESP_LOGE(TAG, "MQTT 客户端启动失败: %s", esp_err_to_name(mqtt_err));
            }

            // 保持任务运行，用于处理后续的网络维护或等待断开事件
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
