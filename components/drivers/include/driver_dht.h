#pragma once

#include <driver/gpio.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 传感器类型枚举
 */
typedef enum
{
    DRIVER_DHT_TYPE_DHT11 = 0,   //!< DHT11
    DRIVER_DHT_TYPE_AM2301,      //!< AM2301 (DHT21, DHT22, AM2302, AM2321)
    DRIVER_DHT_TYPE_SI7021       //!< Itead Si7021
} driver_dht_type_t;

/**
 * @brief 从指定引脚读取传感器数据（整数格式）
 *
 * 湿度和温度以整数返回。
 * 例如：humidity=625 表示 62.5%，temperature=244 表示 24.4 摄氏度
 *
 * @param sensor_type 传感器类型 (DHT11, DHT22 等)
 * @param pin 连接传感器的 GPIO 引脚
 * @param[out] humidity 湿度，百分比 * 10，可为 NULL
 * @param[out] temperature 温度，摄氏度 * 10，可为 NULL
 * @return `ESP_OK` 表示成功
 */
esp_err_t driver_dht_read_data(driver_dht_type_t sensor_type, gpio_num_t pin,
                        int16_t *humidity, int16_t *temperature);

/**
 * @brief 从指定引脚读取传感器数据（浮点格式）
 *
 * 湿度和温度以浮点数返回。
 *
 * @param sensor_type 传感器类型 (DHT11, DHT22 等)
 * @param pin 连接传感器的 GPIO 引脚
 * @param[out] humidity 湿度，百分比，可为 NULL
 * @param[out] temperature 温度，摄氏度，可为 NULL
 * @return `ESP_OK` 表示成功
 */
esp_err_t driver_dht_read_float_data(driver_dht_type_t sensor_type, gpio_num_t pin,
                              float *humidity, float *temperature);

#ifdef __cplusplus
}
#endif
