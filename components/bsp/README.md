# Board Support Package (BSP)

## 用途
本组件用于存放与特定硬件板卡相关的底层驱动和引脚定义。

## 建议内容
- `include/bsp_pin_defs.h`: 所有 GPIO 引脚宏定义
- `bsp_i2c.c/h`: I2C 总线初始化
- `bsp_spi.c/h`: SPI 总线初始化
- `bsp_power.c/h`: 电源管理配置

## 规范
- 此层代码直接调用 ESP-IDF 的 `driver/` 接口。
- 上层应用或其他组件不应直接操作 GPIO 号，而应调用 BSP 提供的接口。
