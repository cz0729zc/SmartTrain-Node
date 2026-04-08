/**
 * @file bsp_pin_defs.h
 * @brief BSP 硬件引脚定义
 */

#ifndef BSP_PIN_DEFS_H
#define BSP_PIN_DEFS_H

#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * SPI 总线引脚 (LCD 和触摸屏共用)
 ******************************************************************************/
#define BSP_SPI_HOST        SPI2_HOST
#define BSP_SPI_MOSI        GPIO_NUM_38
#define BSP_SPI_MISO        GPIO_NUM_41
#define BSP_SPI_CLK         GPIO_NUM_39

/*******************************************************************************
 * LCD 引脚定义 (ILI9488)
 ******************************************************************************/
#define BSP_LCD_CS          GPIO_NUM_35
#define BSP_LCD_DC          GPIO_NUM_37
#define BSP_LCD_RST         GPIO_NUM_36
#define BSP_LCD_BACKLIGHT   GPIO_NUM_40

/* LCD 参数 */
#define BSP_LCD_H_RES       320
#define BSP_LCD_V_RES       480
#define BSP_LCD_BIT_PER_PIXEL   18

/*******************************************************************************
 * 触摸屏引脚定义 (XPT2046)
 ******************************************************************************/
#define BSP_TOUCH_CS        GPIO_NUM_42
#define BSP_TOUCH_INT       GPIO_NUM_NC  /* 不使用中断引脚 */

#ifdef __cplusplus
}
#endif

#endif /* BSP_PIN_DEFS_H */
