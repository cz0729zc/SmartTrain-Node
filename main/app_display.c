/**
 * @file app_display.c
 * @brief 显示模块应用层实现 (测试阶段)
 */

#include "app_display.h"
#include "bsp_lcd.h"
#include "bsp_touch.h"
#include "bsp_pin_defs.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ctype.h>

static const char *TAG = "app_display";

/* RGB565 颜色定义 */
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_WHITE     0xFFFF
#define COLOR_BLACK     0x0000

#define BOOT_FONT_W     5
#define BOOT_FONT_H     7
#define BOOT_FONT_SCALE 3
#define BOOT_LINE_H     34
#define BOOT_TEXT_X     12
#define BOOT_TEXT_Y0    70

static bool s_boot_inited = false;

static const uint8_t *boot_font5x7(char c)
{
    static const uint8_t glyph_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t glyph_qmark[5] = {0x02, 0x01, 0x59, 0x09, 0x06};

    static const uint8_t glyph_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
    static const uint8_t glyph_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
    static const uint8_t glyph_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
    static const uint8_t glyph_3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
    static const uint8_t glyph_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
    static const uint8_t glyph_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
    static const uint8_t glyph_6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
    static const uint8_t glyph_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
    static const uint8_t glyph_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t glyph_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};

    static const uint8_t glyph_A[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
    static const uint8_t glyph_B[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t glyph_C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
    static const uint8_t glyph_D[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
    static const uint8_t glyph_E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
    static const uint8_t glyph_F[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
    static const uint8_t glyph_G[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
    static const uint8_t glyph_H[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
    static const uint8_t glyph_I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
    static const uint8_t glyph_J[5] = {0x20, 0x40, 0x41, 0x3F, 0x01};
    static const uint8_t glyph_K[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
    static const uint8_t glyph_L[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t glyph_M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    static const uint8_t glyph_N[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
    static const uint8_t glyph_O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
    static const uint8_t glyph_P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
    static const uint8_t glyph_Q[5] = {0x3E, 0x41, 0x51, 0x21, 0x5E};
    static const uint8_t glyph_R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
    static const uint8_t glyph_S[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
    static const uint8_t glyph_T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
    static const uint8_t glyph_U[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
    static const uint8_t glyph_V[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
    static const uint8_t glyph_W[5] = {0x3F, 0x40, 0x38, 0x40, 0x3F};
    static const uint8_t glyph_X[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
    static const uint8_t glyph_Y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};
    static const uint8_t glyph_Z[5] = {0x61, 0x51, 0x49, 0x45, 0x43};

    static const uint8_t glyph_colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t glyph_dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t glyph_dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
    static const uint8_t glyph_slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};

    if (c >= 'a' && c <= 'z') {
        c = (char)toupper((unsigned char)c);
    }

    switch (c) {
        case ' ': return glyph_space;
        case '?': return glyph_qmark;
        case ':': return glyph_colon;
        case '-': return glyph_dash;
        case '.': return glyph_dot;
        case '/': return glyph_slash;
        case '0': return glyph_0;
        case '1': return glyph_1;
        case '2': return glyph_2;
        case '3': return glyph_3;
        case '4': return glyph_4;
        case '5': return glyph_5;
        case '6': return glyph_6;
        case '7': return glyph_7;
        case '8': return glyph_8;
        case '9': return glyph_9;
        case 'A': return glyph_A;
        case 'B': return glyph_B;
        case 'C': return glyph_C;
        case 'D': return glyph_D;
        case 'E': return glyph_E;
        case 'F': return glyph_F;
        case 'G': return glyph_G;
        case 'H': return glyph_H;
        case 'I': return glyph_I;
        case 'J': return glyph_J;
        case 'K': return glyph_K;
        case 'L': return glyph_L;
        case 'M': return glyph_M;
        case 'N': return glyph_N;
        case 'O': return glyph_O;
        case 'P': return glyph_P;
        case 'Q': return glyph_Q;
        case 'R': return glyph_R;
        case 'S': return glyph_S;
        case 'T': return glyph_T;
        case 'U': return glyph_U;
        case 'V': return glyph_V;
        case 'W': return glyph_W;
        case 'X': return glyph_X;
        case 'Y': return glyph_Y;
        case 'Z': return glyph_Z;
        default: return glyph_qmark;
    }
}

static void boot_draw_rect(int x, int y, int w, int h, uint16_t color)
{
    esp_lcd_panel_handle_t panel = bsp_lcd_get_panel_handle();
    if (panel == NULL || w <= 0 || h <= 0) {
        return;
    }

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= BSP_LCD_H_RES || y >= BSP_LCD_V_RES) {
        return;
    }
    if (x + w > BSP_LCD_H_RES) {
        w = BSP_LCD_H_RES - x;
    }
    if (y + h > BSP_LCD_V_RES) {
        h = BSP_LCD_V_RES - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    uint16_t line_buf[BSP_LCD_H_RES];
    for (int i = 0; i < w; ++i) {
        line_buf[i] = color;
    }

    for (int yy = y; yy < y + h; ++yy) {
        esp_lcd_panel_draw_bitmap(panel, x, yy, x + w, yy + 1, line_buf);
    }
}

static void boot_draw_char(int x, int y, char c, uint16_t color)
{
    const uint8_t *glyph = boot_font5x7(c);
    for (int col = 0; col < BOOT_FONT_W; ++col) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < BOOT_FONT_H; ++row) {
            if ((bits >> row) & 0x1U) {
                boot_draw_rect(x + col * BOOT_FONT_SCALE,
                               y + row * BOOT_FONT_SCALE,
                               BOOT_FONT_SCALE,
                               BOOT_FONT_SCALE,
                               color);
            }
        }
    }
}

static void boot_draw_text(int x, int y, const char *text, uint16_t color)
{
    if (text == NULL) {
        return;
    }

    int cursor_x = x;
    while (*text != '\0') {
        boot_draw_char(cursor_x, y, *text, color);
        cursor_x += (BOOT_FONT_W + 1) * BOOT_FONT_SCALE;
        ++text;
    }
}

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

esp_err_t app_display_boot_init(void)
{
    if (s_boot_inited) {
        return ESP_OK;
    }

    esp_err_t ret = bsp_lcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Boot LCD init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    bsp_lcd_backlight_set(75);
    s_boot_inited = true;
    return ESP_OK;
}

void app_display_boot_show_title(const char *title)
{
    if (!s_boot_inited) {
        return;
    }

    bsp_lcd_fill_color(APP_DISPLAY_COLOR_BLACK);
    boot_draw_text(BOOT_TEXT_X, 18, "BOOT SELF CHECK", APP_DISPLAY_COLOR_WHITE);
    if (title != NULL && title[0] != '\0') {
        boot_draw_text(BOOT_TEXT_X, 42, title, APP_DISPLAY_COLOR_YELLOW);
    }
}

void app_display_boot_show_line(uint8_t line_index, const char *text, uint16_t color)
{
    if (!s_boot_inited || text == NULL) {
        return;
    }

    int y = BOOT_TEXT_Y0 + ((int)line_index * BOOT_LINE_H);
    if (y >= BSP_LCD_V_RES - 20) {
        return;
    }

    boot_draw_rect(BOOT_TEXT_X, y, BSP_LCD_H_RES - BOOT_TEXT_X * 2, 26, APP_DISPLAY_COLOR_BLACK);
    boot_draw_text(BOOT_TEXT_X, y, text, color);
}
