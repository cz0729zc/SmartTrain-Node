#include "app_time.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if __has_include("esp_netif_sntp.h")
#include "esp_netif_sntp.h"
#define APP_TIME_USE_ESP_NETIF_SNTP 1
#else
#include "lwip/apps/sntp.h"
#define APP_TIME_USE_ESP_NETIF_SNTP 0
#endif

static const char *TAG = "app_time";

/* 2024-01-01 00:00:00 UTC，用于判断时间是否可信 */
#define VALID_UNIX_TIME_BASE (1704067200)

static bool s_time_synced = false;

static bool is_time_valid(time_t now)
{
    return now >= VALID_UNIX_TIME_BASE;
}

esp_err_t app_time_sync_once(uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        timeout_ms = CONFIG_TIME_SYNC_TIMEOUT_MS;
    }

    setenv("TZ", CONFIG_TIME_SYNC_TIMEZONE, 1);
    tzset();

    ESP_LOGI(TAG, "SNTP start: server=%s timeout=%lu ms", CONFIG_TIME_SYNC_NTP_SERVER, (unsigned long)timeout_ms);

#if APP_TIME_USE_ESP_NETIF_SNTP
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_TIME_SYNC_NTP_SERVER);
    esp_err_t ret = esp_netif_sntp_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_sntp_init failed: %s", esp_err_to_name(ret));
        s_time_synced = false;
        return ret;
    }

    ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms));
    if (ret == ESP_OK) {
        time_t now = 0;
        time(&now);
        s_time_synced = is_time_valid(now);
        esp_netif_sntp_deinit();
        ESP_LOGI(TAG, "SNTP sync success, unix=%lld", (long long)now);
        return s_time_synced ? ESP_OK : ESP_FAIL;
    }

    esp_netif_sntp_deinit();
    s_time_synced = false;
    ESP_LOGW(TAG, "SNTP sync timeout");
    return ESP_ERR_TIMEOUT;
#else
    if (sntp_enabled()) {
        sntp_stop();
    }

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, CONFIG_TIME_SYNC_NTP_SERVER);
    sntp_init();

    const TickType_t delay_ticks = pdMS_TO_TICKS(200);
    const uint32_t loops = timeout_ms / 200 + 1;

    for (uint32_t i = 0; i < loops; i++) {
        time_t now = 0;
        time(&now);
        if (is_time_valid(now)) {
            s_time_synced = true;
            sntp_stop();
            ESP_LOGI(TAG, "SNTP sync success, unix=%lld", (long long)now);
            return ESP_OK;
        }
        vTaskDelay(delay_ticks);
    }

    s_time_synced = false;
    sntp_stop();
    ESP_LOGW(TAG, "SNTP sync timeout");
    return ESP_ERR_TIMEOUT;
#endif
}

bool app_time_is_synchronized(void)
{
    return s_time_synced;
}

time_t app_time_now(void)
{
    time_t now = 0;
    time(&now);
    return now;
}

esp_err_t app_time_format_local(char *buf, size_t len)
{
    if (buf == NULL || len < 20) {
        return ESP_ERR_INVALID_ARG;
    }

    time_t now = app_time_now();
    struct tm tm_info;
    if (localtime_r(&now, &tm_info) == NULL) {
        return ESP_FAIL;
    }

    if (strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tm_info) == 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}
