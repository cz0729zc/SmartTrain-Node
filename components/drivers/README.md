# External Device Drivers

## 用途
本组件用于存放外部设备的驱动程序（非 ESP32 内部外设）。

## 建议内容
- 传感器驱动 (e.g., `sensor_sht30.c`)
- 屏幕驱动 (e.g., `lcd_st7789.c`)
- 执行器驱动 (e.g., `motor_driver.c`)

## 规范
- 每个驱动应尽量独立。
- 硬件接口（如 I2C 句柄）应通过初始化函数传入，尽量不直接依赖具体的 BSP I2C 实例，以提高复用性。
