#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MQTT 事件类型
 */
typedef enum {
    APP_MQTT_EVENT_CONNECTED,      // MQTT 连接成功
    APP_MQTT_EVENT_DISCONNECTED,   // MQTT 连接断开
} app_mqtt_event_t;

/**
 * @brief MQTT 事件回调函数类型
 *
 * @param event 事件类型
 * @param arg 用户参数
 */
typedef void (*app_mqtt_event_cb_t)(app_mqtt_event_t event, void *arg);

/**
 * @brief 设置 MQTT 事件回调
 *
 * 上层模块可通过此回调在 MQTT 连接/断开时执行业务逻辑，
 * 如订阅主题、初始化状态等。
 *
 * @param callback 回调函数
 * @param arg 传递给回调的用户参数
 */
void app_mqtt_set_event_callback(app_mqtt_event_cb_t callback, void *arg);

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

#ifdef __cplusplus
}
#endif
