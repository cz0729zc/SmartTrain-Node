/**
 * @file bsp_lcd.c
 * @brief BSP LCD 驱动封装 (ILI9488) 实现
 */

#include "bsp_lcd.h"
#include "bsp_pin_defs.h"

#include <string.h>
#include <esp_log.h>
#include <driver/spi_master.h>
#include <driver/ledc.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_ili9488.h>

static const char *TAG = "bsp_lcd";

/*******************************************************************************
 * 常量定义
 ******************************************************************************/
#define LCD_CMD_BITS        8
#define LCD_PARAM_BITS      8
#define LCD_SPI_QUEUE_LEN   10
#define SPI_MAX_TRANSFER_SIZE   (BSP_LCD_H_RES * 40 * 3)  /* 40行 x 3字节/像素 */

/* 背光 PWM 配置 */
#define BACKLIGHT_LEDC_TIMER        LEDC_TIMER_1
#define BACKLIGHT_LEDC_MODE         LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_CHANNEL      LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_RESOLUTION   LEDC_TIMER_10_BIT
#define BACKLIGHT_LEDC_FREQUENCY    5000

/* SPI 总线频率 */
#define BSP_LCD_SPI_FREQ_HZ         (40 * 1000 * 1000)

/*******************************************************************************
 * 静态变量
 ******************************************************************************/
static esp_lcd_panel_io_handle_t s_lcd_io_handle = NULL;
static esp_lcd_panel_handle_t s_lcd_panel_handle = NULL;
static bool s_spi_initialized = false;

/*******************************************************************************
 * 静态函数
 ******************************************************************************/

/**
 * @brief 初始化 SPI 总线 (LCD 和触摸屏共用)
 */
static esp_err_t spi_bus_init(void)
{
    if (s_spi_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化 SPI 总线 (MOSI:%d, MISO:%d, CLK:%d)",
             BSP_SPI_MOSI, BSP_SPI_MISO, BSP_SPI_CLK);

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_SPI_MOSI,
        .miso_io_num = BSP_SPI_MISO,
        .sclk_io_num = BSP_SPI_CLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = SPI_MAX_TRANSFER_SIZE,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };

    esp_err_t ret = spi_bus_initialize(BSP_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK) {
        s_spi_initialized = true;
    }
    return ret;
}

/**
 * @brief 初始化背光 PWM
 */
static void backlight_init(void)
{
    ESP_LOGI(TAG, "初始化背光 PWM (GPIO:%d)", BSP_LCD_BACKLIGHT);

    ledc_timer_config_t timer_cfg = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_RESOLUTION,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = BACKLIGHT_LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channel_cfg = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
}

/*******************************************************************************
 * 公共函数
 ******************************************************************************/

esp_err_t bsp_lcd_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "初始化 LCD (ILI9488, %dx%d)", BSP_LCD_H_RES, BSP_LCD_V_RES);

    /* 初始化背光 (先关闭) */
    backlight_init();
    bsp_lcd_backlight_set(0);

    /* 初始化 SPI 总线 */
    ret = spi_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI 总线初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 创建 LCD SPI IO */
    ESP_LOGI(TAG, "创建 LCD SPI IO (CS:%d, DC:%d)", BSP_LCD_CS, BSP_LCD_DC);
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = BSP_LCD_CS,
        .dc_gpio_num = BSP_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = BSP_LCD_SPI_FREQ_HZ,
        .trans_queue_depth = LCD_SPI_QUEUE_LEN,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .flags = {
            .dc_low_on_data = 0,
            .octal_mode = 0,
            .sio_mode = 0,
            .lsb_first = 0,
            .cs_high_active = 0,
        },
    };

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_SPI_HOST,
                                    &io_cfg, &s_lcd_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 LCD SPI IO 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 创建 ILI9488 panel */
    ESP_LOGI(TAG, "创建 ILI9488 panel (RST:%d)", BSP_LCD_RST);
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  /* 尝试 BGR 顺序 */
        .bits_per_pixel = BSP_LCD_BIT_PER_PIXEL,
        .flags = {
            .reset_active_high = 0,
        },
        .vendor_config = NULL,
    };

    /* ILI9488 需要提供 buffer_size 用于颜色转换 */
    size_t color_buffer_size = BSP_LCD_H_RES * 40;  /* 40 行缓冲 */
    ret = esp_lcd_new_panel_ili9488(s_lcd_io_handle, &panel_cfg,
                                     color_buffer_size, &s_lcd_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 ILI9488 panel 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化 panel */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_lcd_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_lcd_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_lcd_panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_lcd_panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_lcd_panel_handle, false, false));  /* 镜像由 LVGL port 控制 */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_lcd_panel_handle, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_lcd_panel_handle, true));

    ESP_LOGI(TAG, "LCD 初始化完成");
    return ESP_OK;
}

void bsp_lcd_backlight_set(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }

    /* 10位分辨率: 最大值 1023 */
    uint32_t duty = (1023 * percent) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL));

    ESP_LOGD(TAG, "背光设置为 %d%%", percent);
}

esp_lcd_panel_handle_t bsp_lcd_get_panel_handle(void)
{
    return s_lcd_panel_handle;
}

esp_lcd_panel_io_handle_t bsp_lcd_get_io_handle(void)
{
    return s_lcd_io_handle;
}

void bsp_lcd_fill_color(uint16_t color)
{
    if (s_lcd_panel_handle == NULL) {
        ESP_LOGE(TAG, "LCD 未初始化");
        return;
    }

    /* 分配一行颜色缓冲区 */
    size_t line_size = BSP_LCD_H_RES * sizeof(uint16_t);
    uint16_t *line_buf = heap_caps_malloc(line_size, MALLOC_CAP_DMA);
    if (line_buf == NULL) {
        ESP_LOGE(TAG, "分配行缓冲区失败");
        return;
    }

    /* 填充颜色 */
    for (int i = 0; i < BSP_LCD_H_RES; i++) {
        line_buf[i] = color;
    }

    /* 逐行绘制 */
    for (int y = 0; y < BSP_LCD_V_RES; y++) {
        esp_lcd_panel_draw_bitmap(s_lcd_panel_handle, 0, y, BSP_LCD_H_RES, y + 1, line_buf);
    }

    free(line_buf);
    ESP_LOGI(TAG, "填充颜色 0x%04X 完成", color);
}
