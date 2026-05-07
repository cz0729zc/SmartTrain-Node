#include "app_co2.h"
#include "driver_co2.h"
#include "sensor_data.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "app_co2";

#define APP_CO2_TASK_CORE 1

/* ==================== 硬件配置 ==================== */
#define CO2_UART_NUM        UART_NUM_1
#define CO2_UART_TX_GPIO    GPIO_NUM_4    // 改用 GPIO4 避免与 PSRAM 冲突
#define CO2_UART_RX_GPIO    GPIO_NUM_5    // 改用 GPIO5 避免与 PSRAM 冲突
#define CO2_READ_INTERVAL_MS 2000         // 采集间隔 2 秒
/* ================================================= */

static TaskHandle_t s_co2_task_handle = NULL;
static volatile uint16_t s_last_co2_ppm = 0;
static volatile bool s_data_valid = false;

/**
 * @brief CO2 采集任务
 */
static void co2_task(void *pvParameters)
{
    uint16_t co2_ppm;
    sensor_data_t data = {0};
    ESP_LOGI(TAG, "CO2 采集任务启动...");

    while (1) {
        // 阻塞读取 CO2 数据 (超时 2 秒)
        if (driver_co2_read_timeout(&co2_ppm, 2000) == ESP_OK) {
            s_last_co2_ppm = co2_ppm;
            s_data_valid = true;
            ESP_LOGI(TAG, "CO2: %d ppm", co2_ppm);

            // 通过队列发送给 network_task 上报
            data.co2_ppm = co2_ppm;
            if (sensor_queue_send(&data, 0) != ESP_OK) {
                ESP_LOGW(TAG, "发送 CO2 数据到队列失败 (队列已满)");
            }
        } else {
            ESP_LOGW(TAG, "读取 CO2 数据超时");
        }

        vTaskDelay(pdMS_TO_TICKS(CO2_READ_INTERVAL_MS));
    }
}

esp_err_t app_co2_start(void)
{
    if (s_co2_task_handle != NULL) {
        ESP_LOGW(TAG, "CO2 模块已启动");
        return ESP_ERR_INVALID_STATE;
    }

    // 初始化 CO2 驱动
    driver_co2_config_t config = {
        .uart_num = CO2_UART_NUM,
        .tx_gpio = CO2_UART_TX_GPIO,
        .rx_gpio = CO2_UART_RX_GPIO,
    };

    esp_err_t ret = driver_co2_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CO2 驱动初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 创建采集任务 (栈大小增加到 4096 防止溢出)
#if CONFIG_FREERTOS_UNICORE
    BaseType_t xret = xTaskCreate(co2_task, "co2_task", 4096, NULL, 5, &s_co2_task_handle);
#else
    BaseType_t xret = xTaskCreatePinnedToCore(co2_task,
                                              "co2_task",
                                              4096,
                                              NULL,
                                              5,
                                              &s_co2_task_handle,
                                              APP_CO2_TASK_CORE);
#endif
    if (xret != pdPASS) {
        ESP_LOGE(TAG, "创建 CO2 任务失败");
        driver_co2_deinit();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "CO2 模块启动成功");
    return ESP_OK;
}

esp_err_t app_co2_stop(void)
{
    if (s_co2_task_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    vTaskDelete(s_co2_task_handle);
    s_co2_task_handle = NULL;

    driver_co2_deinit();

    s_data_valid = false;
    s_last_co2_ppm = 0;

    ESP_LOGI(TAG, "CO2 模块已停止");
    return ESP_OK;
}

esp_err_t app_co2_get_value(uint16_t *co2_ppm)
{
    if (co2_ppm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_data_valid) {
        return ESP_ERR_NOT_FOUND;
    }

    *co2_ppm = s_last_co2_ppm;
    return ESP_OK;
}
