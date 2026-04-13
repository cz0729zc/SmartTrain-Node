#include "app_network.h"
#include "app_wifi.h"
#include "app_mqtt.h"
#include "app_time.h"
#include "sensor_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "app_network";

/* OneNET 物模型主题 (业务层定义)
 * 产品ID = MQTT_USERNAME
 * 设备名 = MQTT_CLIENT_ID
 */
#define ONENET_TOPIC_PROPERTY_POST       "$sys/" CONFIG_MQTT_USERNAME "/" CONFIG_MQTT_CLIENT_ID "/thing/property/post"
#define ONENET_TOPIC_PROPERTY_POST_REPLY "$sys/" CONFIG_MQTT_USERNAME "/" CONFIG_MQTT_CLIENT_ID "/thing/property/post/reply"

static TaskHandle_t s_network_task_handle = NULL;

/**
 * @brief MQTT 事件回调 - 处理 OneNET 业务逻辑
 */
static void mqtt_event_callback(app_mqtt_event_t event, void *arg)
{
    switch (event) {
    case APP_MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT 已连接，订阅 OneNET 回复主题");
        app_mqtt_subscribe(ONENET_TOPIC_PROPERTY_POST_REPLY, 0);
        break;
    case APP_MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT 断开连接");
        break;
    }
}

/**
 * @brief 发布传感器数据到 OneNET 平台 (聚合上报)
 */
static int publish_sensor_data(float temperature, float humidity, uint16_t co2_ppm)
{
    char json_buf[256];
    int len = snprintf(json_buf, sizeof(json_buf),
        "{\"id\":\"%s\",\"version\":\"1.0\",\"params\":{"
        "\"humidity\":{\"value\":%.1f},"
        "\"temperature\":{\"value\":%.1f},"
        "\"CO2\":{\"value\":%u}"
        "}}",
        CONFIG_ONENET_DEVICE_ID, humidity, temperature, co2_ppm);

    if (len < 0 || len >= sizeof(json_buf)) {
        ESP_LOGE(TAG, "JSON 数据构建失败");
        return -1;
    }

    ESP_LOGI(TAG, "发布传感器数据: %s", json_buf);
    return app_mqtt_publish(ONENET_TOPIC_PROPERTY_POST, json_buf, len, 0, 0);
}

static void network_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    sensor_data_t cached_data = {0};  // 缓存最新数据
    TickType_t last_no_data_warn_tick = 0;

    ESP_LOGI(TAG, "网络任务启动，开始连接 WiFi...");

    // 1. 初始化 WiFi 组件 (调用 components/app_wifi)
    app_wifi_init();

    // 2. 等待 WiFi 连接成功
    while (app_wifi_wait_connected() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 连接失败，稍后重试...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "WiFi 已连接，准备启动 MQTT...");

    // 3. 联网后执行一次 SNTP 对时（对时后停止 SNTP，避免额外流量）
    esp_err_t time_err = app_time_sync_once(0);
    if (time_err == ESP_OK) {
        char time_buf[32] = {0};
        if (app_time_format_local(time_buf, sizeof(time_buf)) == ESP_OK) {
            ESP_LOGI(TAG, "当前本地时间: %s", time_buf);
        }
    } else {
        ESP_LOGW(TAG, "SNTP 对时失败，后续时间戳可能不准确: %s", esp_err_to_name(time_err));
    }

    // 4. 设置 MQTT 事件回调 (处理 OneNET 业务)
    app_mqtt_set_event_callback(mqtt_event_callback, NULL);

    // 5. 启动 MQTT 客户端
    esp_err_t mqtt_err = app_mqtt_start();
    if (mqtt_err != ESP_OK && mqtt_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "MQTT 客户端启动失败: %s", esp_err_to_name(mqtt_err));
    }

    // 6. 主循环：从队列接收传感器数据，缓存后聚合上报
    while (1) {
        esp_err_t qret;

        // 阻塞等待队列数据，超时 5 秒
        qret = sensor_queue_receive(&sensor_data, 5000);
        if (qret == ESP_OK) {
            // 更新缓存数据
            if (sensor_data.temperature != 0 || sensor_data.humidity != 0) {
                cached_data.temperature = sensor_data.temperature;
                cached_data.humidity = sensor_data.humidity;
            }
            if (sensor_data.co2_ppm != 0) {
                cached_data.co2_ppm = sensor_data.co2_ppm;
            }

            // 检查 MQTT 连接状态后发布
            if (app_mqtt_is_connected()) {
                publish_sensor_data(cached_data.temperature, cached_data.humidity, cached_data.co2_ppm);
            } else {
                ESP_LOGW(TAG, "MQTT 未连接，数据丢弃");
            }
        } else {
            TickType_t now = xTaskGetTickCount();

            // 无传感器/无数据时仅周期性告警，不阻塞主流程
            if (qret == ESP_ERR_INVALID_STATE) {
                if ((now - last_no_data_warn_tick) > pdMS_TO_TICKS(10000)) {
                    ESP_LOGW(TAG, "传感器队列未初始化，当前仅运行网络与时间同步功能");
                    last_no_data_warn_tick = now;
                }
            } else if (qret == ESP_ERR_TIMEOUT) {
                if ((now - last_no_data_warn_tick) > pdMS_TO_TICKS(10000)) {
                    ESP_LOGW(TAG, "暂未收到传感器数据，系统保持运行");
                    last_no_data_warn_tick = now;
                }
            } else {
                ESP_LOGW(TAG, "接收传感器队列异常: %s", esp_err_to_name(qret));
            }

            // 队列无数据或未初始化时避免空转
            vTaskDelay(pdMS_TO_TICKS(100));
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
