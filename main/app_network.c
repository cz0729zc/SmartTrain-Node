#include "app_network.h"
#include "app_wifi.h"
#include "app_perf_monitor.h"
#include "app_mqtt.h"
#include "app_time.h"
#include "attendance_record.h"
#include "sensor_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "app_network";

#define APP_NETWORK_TASK_CORE 0

/* OneNET 物模型主题 (业务层定义)
 * 产品ID = MQTT_USERNAME
 * 设备名 = MQTT_CLIENT_ID
 */
#define ONENET_TOPIC_PROPERTY_POST       "$sys/" CONFIG_MQTT_USERNAME "/" CONFIG_MQTT_CLIENT_ID "/thing/property/post"
#define ONENET_TOPIC_PROPERTY_POST_REPLY "$sys/" CONFIG_MQTT_USERNAME "/" CONFIG_MQTT_CLIENT_ID "/thing/property/post/reply"
#define ONENET_TOPIC_EVENT_POST          "$sys/" CONFIG_MQTT_USERNAME "/" CONFIG_MQTT_CLIENT_ID "/thing/event/post"
#define ONENET_TOPIC_EVENT_POST_REPLY    "$sys/" CONFIG_MQTT_USERNAME "/" CONFIG_MQTT_CLIENT_ID "/thing/event/post/reply"
#define ATTENDANCE_UPLOAD_REPLY_TIMEOUT_MS 15000

static TaskHandle_t s_network_task_handle = NULL;
static volatile bool s_attendance_upload_inflight = false;
static volatile bool s_attendance_upload_reply_ok = false;
static TickType_t s_attendance_upload_sent_tick = 0;

static bool topic_matches(const char *topic, int topic_len, const char *expected)
{
    if (topic == NULL || expected == NULL || topic_len < 0) {
        return false;
    }

    size_t expected_len = strlen(expected);
    return expected_len == (size_t)topic_len && strncmp(topic, expected, expected_len) == 0;
}

static bool json_reply_code_ok(const char *data, int data_len)
{
    if (data == NULL || data_len <= 0) {
        return false;
    }

    char buf[256] = {0};
    size_t copy_len = (size_t)data_len < (sizeof(buf) - 1U) ? (size_t)data_len : (sizeof(buf) - 1U);
    memcpy(buf, data, copy_len);

    const char *code = strstr(buf, "\"code\"");
    if (code == NULL) {
        return false;
    }

    const char *colon = strchr(code, ':');
    if (colon == NULL) {
        return false;
    }

    colon++;
    while (*colon == ' ') {
        colon++;
    }

    return atoi(colon) == 200;
}

static void json_escape_string(const char *src, char *dst, size_t dst_len)
{
    if (dst == NULL || dst_len == 0) {
        return;
    }

    dst[0] = '\0';
    if (src == NULL) {
        return;
    }

    size_t pos = 0;
    for (size_t i = 0; src[i] != '\0' && pos + 1U < dst_len; ++i) {
        char ch = src[i];
        if ((ch == '"' || ch == '\\') && pos + 2U < dst_len) {
            dst[pos++] = '\\';
            dst[pos++] = ch;
        } else if ((unsigned char)ch >= 0x20) {
            dst[pos++] = ch;
        }
    }
    dst[pos] = '\0';
}

static void mqtt_data_callback(const char *topic,
                               int topic_len,
                               const char *data,
                               int data_len,
                               void *arg)
{
    (void)arg;

    if (!topic_matches(topic, topic_len, ONENET_TOPIC_EVENT_POST_REPLY)) {
        return;
    }

    bool ok = json_reply_code_ok(data, data_len);
    ESP_LOGI(TAG,
             "attendance event reply received ok=%d data=%.*s",
             ok,
             data_len,
             data);
    if (ok) {
        s_attendance_upload_reply_ok = true;
    }
}

/**
 * @brief MQTT 事件回调 - 处理 OneNET 业务逻辑
 */
static void mqtt_event_callback(app_mqtt_event_t event, void *arg)
{
    switch (event) {
    case APP_MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT 已连接，订阅 OneNET 回复主题");
        app_mqtt_subscribe(ONENET_TOPIC_PROPERTY_POST_REPLY, 0);
        app_mqtt_subscribe(ONENET_TOPIC_EVENT_POST_REPLY, 0);
        break;
    case APP_MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT 断开连接");
        s_attendance_upload_inflight = false;
        s_attendance_upload_reply_ok = false;
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

static int publish_attendance_event(const attendance_record_item_t *item)
{
    if (item == NULL) {
        return -1;
    }

    long long ts_ms = (long long)item->ts * 1000LL;
    char time_text[ATTENDANCE_RECORD_MAX_TEXT_LEN * 2] = {0};
    char student_id[ATTENDANCE_RECORD_MAX_TEXT_LEN * 2] = {0};
    char card_uid[ATTENDANCE_RECORD_MAX_TEXT_LEN * 2] = {0};
    char method[ATTENDANCE_RECORD_MAX_TEXT_LEN * 2] = {0};
    char result[ATTENDANCE_RECORD_MAX_TEXT_LEN * 2] = {0};
    char reason[ATTENDANCE_RECORD_MAX_TEXT_LEN * 2] = {0};

    json_escape_string(item->time_text, time_text, sizeof(time_text));
    json_escape_string(item->student_id, student_id, sizeof(student_id));
    json_escape_string(item->card_uid, card_uid, sizeof(card_uid));
    json_escape_string(item->method, method, sizeof(method));
    json_escape_string(item->result[0] ? item->result : "ok", result, sizeof(result));
    json_escape_string(item->reason[0] ? item->reason : "matched", reason, sizeof(reason));

    char json_buf[512];
    int len = snprintf(json_buf,
                       sizeof(json_buf),
                       "{\"id\":\"%s\",\"version\":\"1.0\",\"params\":{"
                       "\"attendance_event\":{\"value\":{"
                       "\"ts\":%lld,"
                       "\"time\":\"%s\","
                       "\"student_id\":\"%s\","
                       "\"card_uid\":\"%s\","
                       "\"method\":\"%s\","
                       "\"result\":\"%s\","
                       "\"reason\":\"%s\","
                       "\"face_user_id\":%u,"
                       "\"finger_page_id\":%u"
                       "},\"time\":%lld}}}",
                       CONFIG_ONENET_DEVICE_ID,
                       (long long)item->ts,
                       time_text,
                       student_id,
                       card_uid,
                       method,
                       result,
                       reason,
                       (unsigned)item->face_user_id,
                       (unsigned)item->finger_page_id,
                       ts_ms);

    if (len < 0 || len >= (int)sizeof(json_buf)) {
        ESP_LOGE(TAG, "attendance event JSON build failed");
        return -1;
    }

    ESP_LOGI(TAG,
             "publish attendance event student_id=%s card=%s method=%s payload=%s",
             item->student_id,
             item->card_uid,
             item->method,
             json_buf);
    return app_mqtt_publish(ONENET_TOPIC_EVENT_POST, json_buf, len, 0, 0);
}

static void process_attendance_upload_queue(void)
{
    if (!app_mqtt_is_connected()) {
        return;
    }

    if (s_attendance_upload_reply_ok) {
        esp_err_t drop_ret = attendance_record_pending_drop_first();
        if (drop_ret != ESP_OK && drop_ret != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "drop uploaded attendance pending failed: %s", esp_err_to_name(drop_ret));
            return;
        }
        s_attendance_upload_reply_ok = false;
        s_attendance_upload_inflight = false;
        ESP_LOGI(TAG, "attendance event upload confirmed and pending row removed");
    }

    if (s_attendance_upload_inflight) {
        TickType_t now = xTaskGetTickCount();
        if ((now - s_attendance_upload_sent_tick) > pdMS_TO_TICKS(ATTENDANCE_UPLOAD_REPLY_TIMEOUT_MS)) {
            ESP_LOGW(TAG, "attendance event reply timeout, will retry first pending row");
            s_attendance_upload_inflight = false;
            s_attendance_upload_reply_ok = false;
        } else {
            return;
        }
    }

    attendance_record_item_t item = {0};
    esp_err_t ret = attendance_record_pending_peek(&item);
    if (ret == ESP_ERR_NOT_FOUND || ret == ESP_ERR_INVALID_STATE) {
        return;
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "peek attendance pending failed: %s", esp_err_to_name(ret));
        return;
    }

    int msg_id = publish_attendance_event(&item);
    if (msg_id > 0) {
        s_attendance_upload_inflight = true;
        s_attendance_upload_sent_tick = xTaskGetTickCount();
        ESP_LOGI(TAG, "attendance event upload in-flight msg_id=%d", msg_id);
    } else {
        ESP_LOGW(TAG, "attendance event publish failed, pending row kept");
    }
}

static void network_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    sensor_data_t cached_data = {0};  // 缓存最新数据
    TickType_t last_no_data_warn_tick = 0;

    ESP_LOGI(TAG, "网络任务启动，开始连接 WiFi...");

    // 1. 初始化 WiFi 组件 (调用 components/app_wifi)
    app_wifi_init();
    // esp_err_t perf_err = app_perf_monitor_start();
    // if (perf_err != ESP_OK && perf_err != ESP_ERR_INVALID_STATE) {
    //     ESP_LOGW(TAG, "perf monitor start failed: %s", esp_err_to_name(perf_err));
    // }

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
    app_mqtt_set_data_callback(mqtt_data_callback, NULL);

    // 5. 启动 MQTT 客户端
    esp_err_t mqtt_err = app_mqtt_start();
    if (mqtt_err != ESP_OK && mqtt_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "MQTT 客户端启动失败: %s", esp_err_to_name(mqtt_err));
    }

    // 6. 主循环：从队列接收传感器数据，缓存后聚合上报
    while (1) {
        esp_err_t qret;
        process_attendance_upload_queue();

        // 阻塞等待队列数据，超时 1 秒
        qret = sensor_queue_receive(&sensor_data, 1000);
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

    // 创建网络任务，栈大小分配 6144 字节，因为 MQTT、JSON 构建和 TLS 需要较大栈空间
#if CONFIG_FREERTOS_UNICORE
    BaseType_t ret = xTaskCreate(network_task, "network_task", 6144, NULL, 5, &s_network_task_handle);
#else
    BaseType_t ret = xTaskCreatePinnedToCore(network_task,
                                             "network_task",
                                             6144,
                                             NULL,
                                             5,
                                             &s_network_task_handle,
                                             APP_NETWORK_TASK_CORE);
#endif

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建网络任务失败");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
