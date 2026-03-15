#include "driver_dht.h"

#include <freertos/FreeRTOS.h>
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <rom/ets_sys.h>

// DHT 计时精度（微秒）
#define DHT_TIMER_INTERVAL 2
#define DHT_DATA_BITS 40
#define DHT_DATA_BYTES (DHT_DATA_BITS / 8)

static const char *TAG = "driver_dht";

// 静态互斥锁，用于临界区保护
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
#define PORT_ENTER_CRITICAL() portENTER_CRITICAL(&s_mux)
#define PORT_EXIT_CRITICAL() portEXIT_CRITICAL(&s_mux)

#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

#define CHECK_LOGE(x, msg, ...) do { \
        esp_err_t __; \
        if ((__ = x) != ESP_OK) { \
            PORT_EXIT_CRITICAL(); \
            ESP_LOGE(TAG, msg, ## __VA_ARGS__); \
            return __; \
        } \
    } while (0)

/**
 * @brief 等待引脚达到指定状态
 * 
 * 如果超时且引脚未达到请求状态，则返回错误。
 * 
 * @param pin GPIO 引脚
 * @param timeout 超时时间（微秒）
 * @param expected_pin_state 期望电平 (0 或 1)
 * @param[out] duration 实际持续时间（可选）
 * @return esp_err_t 
 */
static esp_err_t driver_dht_await_pin_state(gpio_num_t pin, uint32_t timeout,
                                     int expected_pin_state, uint32_t *duration)
{
    /* 设置为输入模式以读取电平 */
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    for (uint32_t i = 0; i < timeout; i += DHT_TIMER_INTERVAL)
    {
        // 需要至少等待一个间隔以防止读取抖动
        ets_delay_us(DHT_TIMER_INTERVAL);
        if (gpio_get_level(pin) == expected_pin_state)
        {
            if (duration)
                *duration = i;
            return ESP_OK;
        }
    }

    return ESP_ERR_TIMEOUT;
}

/**
 * @brief 从 DHT 请求数据并读取原始比特流
 * 
 * 此函数调用应受任务切换保护（在临界区内）。
 * 
 * @param sensor_type 传感器类型
 * @param pin GPIO 引脚
 * @param[out] data 读取到的原始数据
 * @return esp_err_t 
 */
static inline esp_err_t driver_dht_fetch_data(driver_dht_type_t sensor_type, gpio_num_t pin, uint8_t data[DHT_DATA_BYTES])
{
    uint32_t low_duration;
    uint32_t high_duration;

// 阶段 'A': 拉低信号以启动读取序列
    // 设置为开漏输出模式
    gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD); 
    gpio_set_level(pin, 0);
    // 根据传感器类型等待不同的时间
    ets_delay_us(sensor_type == DRIVER_DHT_TYPE_SI7021 ? 500 : 20000);
    gpio_set_level(pin, 1);

    // 阶段 'B': 40us
    // DHT 响应信号 Low: 80us, High: 80us. 
    // Wait for the pin to go LOW (end of start signal acknowledgment)
    CHECK_LOGE(driver_dht_await_pin_state(pin, 40, 0, NULL),
               "初始化错误，阶段 'B' 问题(Wait for Low)");
    // Step through Phase 'C', 88us
    // Wait for the pin to go HIGH (Low 80us finished)
    CHECK_LOGE(driver_dht_await_pin_state(pin, 88, 1, NULL),
               "初始化错误，阶段 'C' 问题(Wait for High)");
    // Step through Phase 'D', 88us
    // Wait for the pin to go LOW (High 80us finished) - start of data transmission
    CHECK_LOGE(driver_dht_await_pin_state(pin, 88, 0, NULL),
               "初始化错误，阶段 'D' 问题(Wait for Start Bit Low)");

    // 读取 40 位数据...
    for (int i = 0; i < DHT_DATA_BITS; i++)
    {
        // 等待低电平结束
        CHECK_LOGE(driver_dht_await_pin_state(pin, 65, 1, &low_duration),
                   "低电平位超时");
        // 等待高电平结束（高电平持续时间决定是 0 还是 1）
        CHECK_LOGE(driver_dht_await_pin_state(pin, 75, 0, &high_duration),
                   "高电平位超时");

        uint8_t b = i / 8;
        uint8_t m = i % 8;
        if (!m)
            data[b] = 0;

        // 如果高电平时间长于低电平，则为 1
        if (high_duration > low_duration) {
            data[b] |= (1 << (7 - m));
        }
    }

    return ESP_OK;
}

/**
 * @brief 将两个数据字节打包成单个值并考虑符号位
 */
static inline int16_t driver_dht_convert_data(driver_dht_type_t sensor_type, uint8_t msb, uint8_t lsb)
{
    int16_t data;

    if (sensor_type == DRIVER_DHT_TYPE_DHT11)
    {
        data = msb * 10; // DHT11 只返回整数部分，这里放大10倍以保持一致性
    }
    else
    {
        data = msb & 0x7F;
        data <<= 8;
        data |= lsb;
        if (msb & (1 << 7))
            data = -data;       // 转换为负值
    }

    return data;
}

esp_err_t driver_dht_read_data(driver_dht_type_t sensor_type, gpio_num_t pin,
                        int16_t *humidity, int16_t *temperature)
{
    CHECK_ARG(humidity || temperature);

    uint8_t data[DHT_DATA_BYTES] = { 0 };

    // 初始状态保持高电平
    gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(pin, 1);

    // 进入临界区，防止任务调度打断时序
    PORT_ENTER_CRITICAL();
    esp_err_t result = driver_dht_fetch_data(sensor_type, pin, data);
    // 无论成功失败，都需要退出临界区
    PORT_EXIT_CRITICAL();

    /* 恢复 GPIO 方向，因为调用 fetch_data 后 GPIO 可能处于输入状态 */
    gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(pin, 1);

    if (result != ESP_OK)
        return result;

    // 校验和检查: byte_5 == (byte_1 + byte_2 + byte_3 + byte_4) & 0xFF
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
    {
        ESP_LOGE(TAG, "校验和失败，从传感器接收到无效数据");
        return ESP_ERR_INVALID_CRC; // 使用 ESP-IDF 定义的 CRC 错误码
    }

    if (humidity)
        *humidity = driver_dht_convert_data(sensor_type, data[0], data[1]);
    if (temperature)
        *temperature = driver_dht_convert_data(sensor_type, data[2], data[3]);

    ESP_LOGD(TAG, "传感器数据: humidity=%d, temp=%d", *humidity, *temperature);

    return ESP_OK;
}

esp_err_t driver_dht_read_float_data(driver_dht_type_t sensor_type, gpio_num_t pin,
                              float *humidity, float *temperature)
{
    CHECK_ARG(humidity || temperature);

    int16_t i_humidity, i_temp;

    esp_err_t res = driver_dht_read_data(sensor_type, pin, humidity ? &i_humidity : NULL, temperature ? &i_temp : NULL);
    if (res != ESP_OK)
        return res;

    if (humidity)
        *humidity = i_humidity / 10.0f;
    if (temperature)
        *temperature = i_temp / 10.0f;

    return ESP_OK;
}
