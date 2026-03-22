#include "app_mqtt.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_event.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "app_mqtt";

/* MQTT 客户端句柄 */
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

/* MQTT 连接状态 */
static volatile bool s_mqtt_connected = false;

/**
 * @brief 打印非零错误码
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/**
 * @brief MQTT 事件处理回调
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT 已连接到 Broker");
        s_mqtt_connected = true;
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT 连接断开");
        s_mqtt_connected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "订阅成功, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "取消订阅成功, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "消息发布成功, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "收到消息:");
        ESP_LOGI(TAG, "  主题: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "  数据: %.*s", event->data_len, event->data);
        // TODO: 可在此处添加回调函数，将数据传递给其他模块处理
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT 错误事件");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("esp-tls 报告", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("tls 堆栈报告", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGE(TAG, "错误描述: %s", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "正在连接到 MQTT Broker...");
        break;

    default:
        ESP_LOGD(TAG, "其他事件, id=%d", (int)event_id);
        break;
    }
}

esp_err_t app_mqtt_start(void)
{
    if (s_mqtt_client != NULL) {
        ESP_LOGW(TAG, "MQTT 客户端已启动");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "初始化 MQTT 客户端...");

    /* 配置 MQTT 客户端 */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URL,
        .broker.address.port = CONFIG_MQTT_BROKER_PORT,
        .credentials.username = strlen(CONFIG_MQTT_USERNAME) > 0 ? CONFIG_MQTT_USERNAME : NULL,
        .credentials.authentication.password = strlen(CONFIG_MQTT_PASSWORD) > 0 ? CONFIG_MQTT_PASSWORD : NULL,
        .credentials.client_id = strlen(CONFIG_MQTT_CLIENT_ID) > 0 ? CONFIG_MQTT_CLIENT_ID : NULL,
    };

    /* 创建客户端 */
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "创建 MQTT 客户端失败");
        return ESP_FAIL;
    }

    /* 注册事件处理 */
    esp_err_t ret = esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 MQTT 事件失败: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return ret;
    }

    /* 启动客户端 */
    ret = esp_mqtt_client_start(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动 MQTT 客户端失败: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "MQTT 客户端启动成功");
    ESP_LOGI(TAG, "  Broker: %s", CONFIG_MQTT_BROKER_URL);
    if (strlen(CONFIG_MQTT_CLIENT_ID) > 0) {
        ESP_LOGI(TAG, "  Client ID: %s", CONFIG_MQTT_CLIENT_ID);
    }
    if (strlen(CONFIG_MQTT_USERNAME) > 0) {
        ESP_LOGI(TAG, "  Username: %s", CONFIG_MQTT_USERNAME);
    }
    return ESP_OK;
}

esp_err_t app_mqtt_stop(void)
{
    if (s_mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_mqtt_client_stop(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "停止 MQTT 客户端失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_mqtt_client_destroy(s_mqtt_client);
    s_mqtt_client = NULL;
    s_mqtt_connected = false;

    ESP_LOGI(TAG, "MQTT 客户端已停止");
    return ret;
}

int app_mqtt_publish(const char *topic, const char *data, int len, int qos, int retain)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT 客户端未初始化");
        return -1;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, data, len, qos, retain);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "发布消息失败, topic=%s", topic);
    } else {
        ESP_LOGD(TAG, "发布消息成功, topic=%s, msg_id=%d", topic, msg_id);
    }
    return msg_id;
}

int app_mqtt_subscribe(const char *topic, int qos)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT 客户端未初始化");
        return -1;
    }

    int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "订阅失败, topic=%s", topic);
    } else {
        ESP_LOGI(TAG, "订阅请求已发送, topic=%s, msg_id=%d", topic, msg_id);
    }
    return msg_id;
}

int app_mqtt_unsubscribe(const char *topic)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT 客户端未初始化");
        return -1;
    }

    int msg_id = esp_mqtt_client_unsubscribe(s_mqtt_client, topic);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "取消订阅失败, topic=%s", topic);
    } else {
        ESP_LOGI(TAG, "取消订阅请求已发送, topic=%s, msg_id=%d", topic, msg_id);
    }
    return msg_id;
}

bool app_mqtt_is_connected(void)
{
    return s_mqtt_connected;
}
