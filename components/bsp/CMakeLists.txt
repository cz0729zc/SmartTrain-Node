idf_component_register(
    SRCS "bsp_lcd.c" "bsp_touch.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd esp_timer
    PRIV_REQUIRES esp_lcd_ili9488 esp_lcd_touch_xpt2046
)
