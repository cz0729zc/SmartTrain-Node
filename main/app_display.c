/**
 * @file app_display.c
 * @brief 显示模块应用层实现 (测试阶段)
 */

#include "app_display.h"
#include "bsp_lcd.h"
#include "bsp_touch.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "app_display";

/* RGB565 颜色定义 */
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_WHITE     0xFFFF
#define COLOR_BLACK     0x0000

esp_err_t app_display_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing display...");

    /* 初始化 LCD */
    ret = bsp_lcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化触摸屏 */
    ret = bsp_touch_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 打开背光 */
    bsp_lcd_backlight_set(75);

    ESP_LOGI(TAG, "Display initialized successfully");

    return ESP_OK;
}

void app_display_test(void)
{
    static const uint16_t colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE};
    static const char *color_names[] = {"RED", "GREEN", "BLUE"};
    int color_idx = 0;

    ESP_LOGI(TAG, "Starting display test...");
    ESP_LOGI(TAG, "Touch the screen to see coordinates");
    ESP_LOGI(TAG, "Colors will cycle every 3 seconds");

    TickType_t last_color_change = xTaskGetTickCount();
    const TickType_t color_interval = pdMS_TO_TICKS(3000);

    while (1) {
        /* 每 3 秒切换颜色 */
        if ((xTaskGetTickCount() - last_color_change) >= color_interval) {
            ESP_LOGI(TAG, "Filling screen with %s", color_names[color_idx]);
            bsp_lcd_fill_color(colors[color_idx]);

            color_idx = (color_idx + 1) % 3;
            last_color_change = xTaskGetTickCount();
        }

        /* 读取触摸 */
        uint16_t touch_x, touch_y;
        if (bsp_touch_read(&touch_x, &touch_y)) {
            ESP_LOGI(TAG, "Touch: x=%d, y=%d", touch_x, touch_y);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
