#include "app_network.h"
#include "app_wifi.h"
#include "app_mqtt.h"
#include "sensor_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "app_network";

static TaskHandle_t s_network_task_handle = NULL;

static void network_task(void *pvParameters)
{
    sensor_data_t sensor_data;

    ESP_LOGI(TAG, "网络任务启动，开始连接 WiFi...");

    // 1. 初始化 WiFi 组件 (调用 components/app_wifi)
    app_wifi_init();

    // 2. 等待 WiFi 连接成功
    while (app_wifi_wait_connected() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 连接失败，稍后重试...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "WiFi 已连接，准备启动 MQTT...");

    // 3. 启动 MQTT 客户端
    esp_err_t mqtt_err = app_mqtt_start();
    if (mqtt_err != ESP_OK && mqtt_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "MQTT 客户端启动失败: %s", esp_err_to_name(mqtt_err));
    }

    // 4. 主循环：从队列接收传感器数据并发布到 MQTT
    while (1) {
        // 阻塞等待队列数据，超时 5 秒
        if (sensor_queue_receive(&sensor_data, 5000) == ESP_OK) {
            // 检查 MQTT 连接状态后发布
            if (app_mqtt_is_connected()) {
                app_mqtt_publish_sensor_data(sensor_data.temperature, sensor_data.humidity);
            } else {
                ESP_LOGW(TAG, "MQTT 未连接，数据丢弃");
            }
        }
        // 超时则继续循环，检查其他状态
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
