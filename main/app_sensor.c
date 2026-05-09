#include "app_sensor.h"
#include "sensor_data.h"
#include "driver_dht.h"
#include "events_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <driver/gpio.h>

static const char *TAG = "app_sensor";

#define APP_SENSOR_TASK_CORE 1

// 传感器配置
#define DHT_SENSOR_TYPE DRIVER_DHT_TYPE_DHT11
#define SENSOR_GPIO GPIO_NUM_6
#define SENSOR_READ_INTERVAL_MS 2000  // 采集间隔 2 秒
#define SENSOR_QUEUE_LEN 8
#define SENSOR_MAX_CONSECUTIVE_FAILURES 3

static TaskHandle_t s_sensor_task_handle = NULL;

esp_err_t app_sensor_probe(float *humidity, float *temperature)
{
    return driver_dht_read_float_data(DHT_SENSOR_TYPE, SENSOR_GPIO, humidity, temperature);
}

static void sensor_task(void *pvParameters)
{
    sensor_data_t data = {0};
    uint32_t consecutive_failures = 0;
    ESP_LOGI(TAG, "DHT11 传感器任务启动");

    while (1) {
        data = (sensor_data_t){0};
        esp_err_t ret = driver_dht_read_float_data(DHT_SENSOR_TYPE,
                                                   SENSOR_GPIO,
                                                   &data.humidity,
                                                   &data.temperature);
        if (ret == ESP_OK) {
            consecutive_failures = 0;
            ESP_LOGI(TAG, "DHT11 温度: %.1fC, 湿度: %.1f%%", data.temperature, data.humidity);
            events_standby_update_environment(data.temperature, data.humidity, true);

            // 发送到队列，由 network_task 通过 MQTT 上报。
            if (sensor_queue_send(&data, 0) != ESP_OK) {
                ESP_LOGW(TAG, "发送 DHT11 数据到队列失败，队列可能已满");
            }
        } else {
            consecutive_failures++;
            if (consecutive_failures == 1 ||
                (consecutive_failures % SENSOR_MAX_CONSECUTIVE_FAILURES) == 0) {
                ESP_LOGW(TAG, "DHT11 读取失败(%u): %s",
                         (unsigned)consecutive_failures,
                         esp_err_to_name(ret));
            }
            if (consecutive_failures >= SENSOR_MAX_CONSECUTIVE_FAILURES) {
                events_standby_update_environment(0.0f, 0.0f, false);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

esp_err_t app_sensor_start(void)
{
    if (s_sensor_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE; // 任务已存在
    }

    esp_err_t qret = sensor_queue_init(SENSOR_QUEUE_LEN);
    if (qret != ESP_OK && qret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "初始化传感器队列失败: %s", esp_err_to_name(qret));
        return qret;
    }

#if CONFIG_FREERTOS_UNICORE
    BaseType_t ret = xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, &s_sensor_task_handle);
#else
    BaseType_t ret = xTaskCreatePinnedToCore(sensor_task,
                                             "sensor_task",
                                             4096,
                                             NULL,
                                             5,
                                             &s_sensor_task_handle,
                                             APP_SENSOR_TASK_CORE);
#endif

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建传感器任务失败");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
