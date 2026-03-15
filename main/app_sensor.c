#include "app_sensor.h"
#include "driver_dht.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <driver/gpio.h>

static const char *TAG = "app_sensor";

// 传感器配置
#define SENSOR_TYPE DRIVER_DHT_TYPE_DHT11
#define SENSOR_GPIO GPIO_NUM_4  

static TaskHandle_t s_sensor_task_handle = NULL;

static void sensor_task(void *pvParameters)
{
    float temperature, humidity;
    ESP_LOGI(TAG, "传感器任务启动...");

    while (1)
    {
        // 调用底层驱动读取数据
        if (driver_dht_read_float_data(SENSOR_TYPE, SENSOR_GPIO, &humidity, &temperature) == ESP_OK) {
            ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC", humidity, temperature);
            // TODO: 将数据发送到事件队列
        } else {
            ESP_LOGW(TAG, "无法从传感器读取数据");
        }
        
        // 采集周期：2秒
        vTaskDelay(pdMS_TO_TICKS(2000));
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
