#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化并启动 MQTT 客户端
 *
 * 创建 MQTT 客户端，注册事件处理程序，并连接到 Broker。
 * 需要在 WiFi 连接成功后调用。
 *
 * @return esp_err_t ESP_OK 表示成功，其他表示失败
 */
esp_err_t app_mqtt_start(void);

/**
 * @brief 停止 MQTT 客户端
 *
 * 断开连接并释放资源。
 *
 * @return esp_err_t ESP_OK 表示成功，其他表示失败
 */
esp_err_t app_mqtt_stop(void);

/**
 * @brief 发布消息到指定主题
 *
 * @param topic 主题字符串
 * @param data 消息内容
 * @param len 消息长度，若为 0 则自动计算字符串长度
 * @param qos QoS 等级 (0, 1, 2)
 * @param retain 是否保留消息
 *
 * @return int 消息 ID (>0 成功), -1 失败, -2 发送队列已满
 */
int app_mqtt_publish(const char *topic, const char *data, int len, int qos, int retain);

/**
 * @brief 订阅指定主题
 *
 * @param topic 主题字符串
 * @param qos QoS 等级 (0, 1, 2)
 *
 * @return int 消息 ID (>0 成功), -1 失败
 */
int app_mqtt_subscribe(const char *topic, int qos);

/**
 * @brief 取消订阅指定主题
 *
 * @param topic 主题字符串
 *
 * @return int 消息 ID (>0 成功), -1 失败
 */
int app_mqtt_unsubscribe(const char *topic);

/**
 * @brief 检查 MQTT 是否已连接
 *
 * @return true 已连接
 * @return false 未连接
 */
bool app_mqtt_is_connected(void);

/**
 * @brief 发布传感器数据到 OneNET 平台
 *
 * 将温湿度数据以 OneNET 物模型格式发布到云平台。
 * 主题: $sys/{产品ID}/{设备名}/thing/property/post
 *
 * @param temperature 温度值 (摄氏度)
 * @param humidity 湿度值 (百分比)
 *
 * @return int 消息 ID (>=0 成功), -1 失败
 */
int app_mqtt_publish_sensor_data(float temperature, float humidity);

#ifdef __cplusplus
}
#endif
