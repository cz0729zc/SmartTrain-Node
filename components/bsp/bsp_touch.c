/**
 * @file bsp_touch.c
 * @brief BSP 触摸屏驱动封装 (XPT2046) 实现
 */

#include "bsp_touch.h"
#include "bsp_pin_defs.h"

#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_touch_xpt2046.h>

static const char *TAG = "bsp_touch";

/*******************************************************************************
 * 静态变量
 ******************************************************************************/
static esp_lcd_panel_io_handle_t s_touch_io_handle = NULL;
static esp_lcd_touch_handle_t s_touch_handle = NULL;

/*******************************************************************************
 * 公共函数
 ******************************************************************************/

esp_err_t bsp_touch_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "初始化触摸屏 (XPT2046, CS:%d)", BSP_TOUCH_CS);

    /* 创建触摸屏 SPI IO */
    esp_lcd_panel_io_spi_config_t io_cfg = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(BSP_TOUCH_CS);

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_SPI_HOST,
                                    &io_cfg, &s_touch_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建触摸屏 SPI IO 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置触摸屏 */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = BSP_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .process_coordinates = NULL,
        .interrupt_callback = NULL,
        .user_data = NULL,
        .driver_data = NULL,
    };

    ret = esp_lcd_touch_new_spi_xpt2046(s_touch_io_handle, &tp_cfg, &s_touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 XPT2046 触摸驱动失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "触摸屏初始化完成");
    return ESP_OK;
}

bool bsp_touch_read(uint16_t *x, uint16_t *y)
{
    static uint32_t debug_cnt = 0;

    if (s_touch_handle == NULL) {
        return false;
    }

    /* 更新触摸数据 */
    esp_err_t ret = esp_lcd_touch_read_data(s_touch_handle);
    if (ret != ESP_OK) {
        /* 每 100 次打印一次错误，避免刷屏 */
        if (debug_cnt++ % 100 == 0) {
            ESP_LOGW(TAG, "触摸读取失败: %s", esp_err_to_name(ret));
        }
        return false;
    }

    /* 获取坐标 */
    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;

    bool touched = esp_lcd_touch_get_coordinates(s_touch_handle,
                                                  touch_x, touch_y,
                                                  touch_strength, &touch_cnt, 1);

    /* 调试：每 100 次打印一次状态 */
    if (debug_cnt++ % 100 == 0) {
        ESP_LOGI(TAG, "[DEBUG] touched=%d, cnt=%d, x=%d, y=%d, strength=%d",
                 touched, touch_cnt, touch_x[0], touch_y[0], touch_strength[0]);
    }

    if (touched && touch_cnt > 0) {
        if (x) *x = touch_x[0];
        if (y) *y = touch_y[0];
        return true;
    }

    return false;
}

esp_lcd_touch_handle_t bsp_touch_get_handle(void)
{
    return s_touch_handle;
}
