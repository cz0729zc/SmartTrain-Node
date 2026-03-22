#include "app_sensor.h"
#include "sensor_data.h"
#include "driver_dht.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <driver/gpio.h>

static const char *TAG = "app_sensor";

// 传感器配置
#define SENSOR_TYPE DRIVER_DHT_TYPE_DHT11
#define SENSOR_GPIO GPIO_NUM_4
#define SENSOR_READ_INTERVAL_MS 5000  // 采集间隔 5 秒

static TaskHandle_t s_sensor_task_handle = NULL;

static void sensor_task(void *pvParameters)
{
    sensor_data_t data;
    ESP_LOGI(TAG, "传感器任务启动...");

    while (1)
    {
        // 调用底层驱动读取数据
        if (driver_dht_read_float_data(SENSOR_TYPE, SENSOR_GPIO, &data.humidity, &data.temperature) == ESP_OK) {
            ESP_LOGI(TAG, "温度: %.1f°C, 湿度: %.1f%%", data.temperature, data.humidity);

            // 发送到队列，由 network_task 消费
            if (sensor_queue_send(&data, 0) != ESP_OK) {
                ESP_LOGW(TAG, "发送数据到队列失败 (队列已满)");
            }
        } else {
            ESP_LOGW(TAG, "无法从传感器读取数据");
        }

        // 采集周期
        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

esp_err_t app_sensor_start(void)
{
    if (s_sensor_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE; // 任务已存在
    }

    BaseType_t ret = xTaskCreate(sensor_task, "sensor_task", 2048, NULL, 5, &s_sensor_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建传感器任务失败");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
