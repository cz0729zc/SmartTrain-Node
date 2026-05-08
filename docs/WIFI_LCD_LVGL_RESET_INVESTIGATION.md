# WiFi + LCD + LVGL 复位问题排查记录

## 背景

目标硬件/软件组合：

- ESP32-S3
- ESP-IDF v5.4.3
- LVGL 9.5
- SPI LCD: ILI9488, 320x480, 18-bit panel path
- Touch: XPT2046
- WiFi STA + 后续网络任务

现象演进：

- UI 单独运行正常。
- WiFi 单独运行正常。
- UI + WiFi 合并后出现复位，早期表现为白屏或启动阶段 `TG1WDT_SYS_RST`。
- 后续优化后第一个界面可以显示，但切屏到 `components/ui_custom/setup_scr_screen_Standby.c` 时崩溃。
- 再之后出现启动阶段在 LVGL display 注册附近复位。

核心判断：这不是单纯的“CPU 使用率太高”，而是 UI、LCD、WiFi 同时使用 ESP32-S3 内部 RAM、DMA-capable RAM、SPI LCD 异步传输、LVGL task/lock 时序后暴露出来的组合问题。

## 已排查和基本排除的原因

### 1. CPU 占用过高不是主因

证据：

- 性能日志中两个 IDLE task 占比很高。
- LVGL 约 9% 左右。
- WiFi task 占比很低。
- 崩溃时不是持续满 CPU，而是在特定初始化/显示注册/切屏路径触发。

结论：

- CPU runtime 高不是复位主因。
- WDT 是结果，不是根因本身。
- 更可能是某个任务/中断/驱动路径卡死、内存破坏、或异步 SPI/LVGL 时序异常导致系统最终被 WDT 复位。

### 2. 单纯“WiFi 启动后内存不足”不是当前唯一原因

早期证据：

- `esp_lcd_new_panel_ili9488()` 曾失败：

```text
Failed to allocate DMA color conversion buffer
ESP_ERR_NO_MEM
```

- WiFi 初始化后会消耗大量 internal/DMA-capable memory。
- LCD ILI9488 18-bit SPI 路径需要额外 RGB565 -> RGB666 DMA 转换缓冲区。

这说明：

- WiFi + LCD 的确存在 internal/DMA 连续内存竞争。
- 初始化顺序和连续内存碎片会影响 LCD panel 分配。

但后续日志显示：

```text
before lvgl_port_add_disp internal=272259 internal_largest=192512 dma=264471 dma_largest=192512
```

此时：

- WiFi 还没有启动。
- internal/DMA 最大连续块仍很大。
- 仍然在 display 注册后复位。

结论：

- “WiFi 抢内存导致 LCD 无法分配”是历史上真实存在的问题之一。
- 但当前卡在 `lvgl_port_add_disp()` 之后的复位，不能再只按内存不足解释。

### 3. LVGL built-in malloc 静态池问题已排查

曾发现：

- LVGL 若使用 built-in malloc，会通过 `LV_ATTRIBUTE_LARGE_RAM_ARRAY` 创建一块静态内存池。
- 原先 `CONFIG_LV_MEM_SIZE_KILOBYTES=128` 会占用大量内部 RAM。
- 这会显著加剧 WiFi + LCD + LVGL 合并后的 internal RAM 压力。

当前状态：

```text
CONFIG_LV_USE_CLIB_MALLOC=y
# CONFIG_LV_USE_BUILTIN_MALLOC is not set
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```

结论：

- LVGL 128KB built-in 静态池已经不是当前日志中的直接原因。
- 这个配置调整显著释放了 internal RAM，是必要的全局优化。

注意：

- `sdkconfig` 后续由 menuconfig 修改。
- 代码侧不直接编辑 `sdkconfig`。

### 4. “LCD 不使用 DMA”不可作为根本解决方案

ILI9488 当前使用 18-bit SPI 写入路径：

- LVGL 输出 RGB565。
- ILI9488 SPI 18-bit 模式需要转换为 RGB666。
- `managed_components/atanisoft__esp_lcd_ili9488/esp_lcd_ili9488.c` 内部会分配 `MALLOC_CAP_DMA` 的 `color_buffer`。
- `panel_ili9488_draw_bitmap()` 会把 RGB565 转成 RGB666 后通过 `esp_lcd_panel_io_tx_color()` 发送。

结论：

- 这个 panel 驱动路径本质需要 DMA-capable 转换缓冲。
- LVGL draw buffer 可以放 PSRAM，并且不要求 DMA。
- 但 ILI9488 转换缓冲仍然需要 DMA-capable memory。
- 所以“LCD 完全不使用 DMA”不是现实方案，除非换 panel 驱动策略或换成真正支持 RGB565 SPI 写入的路径。

### 5. ILI9488 转换缓冲越界风险已识别

关键隐患：

- ILI9488 驱动的 `panel_ili9488_draw_bitmap()` 根据本次 flush 面积计算 `color_data_len`。
- 然后向 `color_buffer` 写入 `color_data_len * 3` 字节。
- 驱动内部没有检查 `color_data_len` 是否超过初始化时传入的 `buffer_size`。

因此必须保证：

```text
LVGL 单次 flush 像素数 <= ILI9488 color conversion buffer pixels
```

当前代码方向：

- LVGL draw buffer 行数缩小到 5 行。
- ILI9488 color conversion buffer 也按 5 行匹配。
- 逻辑横屏宽度是 480，所以 5 行为 `480 * 5 = 2400` pixels。

风险：

- 这种“两个模块都写 5 行”的耦合比较脆弱。
- 如果 LVGL 因 rotation、full refresh、screen transition 或 invalidate 区域变化发出超过 2400 pixels 的 flush，就可能写越界。
- 越界后可能表现为 FreeRTOS 链表损坏、`LoadProhibited`、栈回溯损坏、或后续 WDT，而不一定在越界现场崩溃。

结论：

- 这是全局根因候选之一。
- 后续应把这个约束显式化，不能靠两个宏“刚好相等”维持。

### 6. 网络任务重复初始化 WiFi 已排查

早期结构中：

- `main.c` 可能启动 WiFi。
- `app_network.c` 的 `network_task` 内也会调用 `app_wifi_init()`。

这会造成：

- netif/event loop/event handler 生命周期不清晰。
- WiFi 驱动和网络服务初始化边界重复。
- 和 UI/LVGL 初始化阶段叠加，增加不确定性。

当前方向：

- WiFi 初始化由 `app_main()` 统一负责。
- `app_network_start()` 只启动网络服务任务。
- 网络任务不再重复初始化 WiFi。

结论：

- 重复初始化不是当前唯一崩溃点，但属于架构层面的真实问题。
- 需要保持单一生命周期所有权。

### 7. UI 切屏本身不是最早根因，但仍是后续风险点

曾出现：

- 第一个界面能显示。
- 点击返回按钮后进入 `setup_scr_screen_Standby.c` 崩溃。

这个阶段可能涉及：

- 在 LVGL task 外调用 LVGL API 未加锁。
- event 回调中直接切屏。
- Standby screen 创建对象较多，引发更大 invalidate/flush 面积。
- 切屏时旧对象删除、新对象创建、动画/刷新与 SPI flush 重叠。
- ILI9488 转换缓冲不足导致刷新区域越界。

结论：

- Standby 切屏可能是触发点。
- 但当前启动阶段已经在 display 注册处复位，所以要先解决 LVGL display attach/flush 基础层。
- 基础层稳定后，再回到 `events_init.c` 和 `setup_scr_screen_Standby.c` 检查 LVGL 锁、异步切屏、对象生命周期。

## 当前最可疑方向

### 1. `esp_lvgl_port` 的 SPI display 注册路径

当前日志卡点：

```text
before lvgl_port_add_disp
```

随后直接复位，没有正常输出：

```text
after lvgl_port_add_disp
```

`lvgl_port_add_disp()` 内部做了多件事：

- 加 LVGL lock。
- 分配 draw buffer。
- 创建 `lv_display_t`。
- 设置 flush callback。
- 注册 LCD IO `on_color_trans_done` 回调。
- 执行 display rotation update。
- rotation update 会调用 `esp_lcd_panel_swap_xy()` 和 `esp_lcd_panel_mirror()`。
- 最后唤醒 LVGL task。

对 SPI LCD 来说，这些操作混在一起不利于定位：

- 注册了 flush-ready IO callback 后，又发送非 flush 的 LCD command。
- LVGL task 已经启动，display 注册期间又可能被唤醒。
- 第三方 ILI9488 驱动没有转换缓冲边界保护。

当前代码方向：

- 保留 `lvgl_port_init()` 的 LVGL task/tick/lock。
- 绕开 `lvgl_port_add_disp()`。
- 应用层自己创建 `lv_display_t`。
- 应用层自己分配 PSRAM draw buffer。
- 应用层自己注册 flush callback 和 IO 完成 callback。
- LCD 横屏 `swap_xy/mirror` 在 LVGL display 创建前直接设置。

目的：

- 拆开黑盒路径。
- 避免 display 注册期间同时做 rotation、IO callback、任务唤醒。
- 让后续日志能精确定位在 display create、buffer set、callback register、touch add、还是首帧 flush。

### 2. SPI/LCD flush 面积和 ILI9488 转换缓冲大小必须建立硬约束

需要继续验证：

- LVGL 首帧 flush 面积是否超过 ILI9488 转换缓冲。
- 切屏时 flush 面积是否扩大。
- LVGL partial render mode 是否严格限制到 draw buffer 大小。
- rotation 后的 area 是否仍和转换缓冲约束一致。

如果仍崩溃，应优先加日志到 flush callback：

```text
area x1/y1/x2/y2
pixel count
conversion buffer capacity
```

并在超过容量时拒绝 flush 或拆分 flush，而不是继续让 ILI9488 驱动越界写。

## 已确认的有效优化

### 1. LVGL 使用 C malloc

目的：

- 移除 LVGL built-in 128KB 静态池对 internal RAM 的长期占用。

当前状态：

```text
CONFIG_LV_USE_CLIB_MALLOC=y
```

对应 SDKCONFIG 调整：

```text
# CONFIG_LV_USE_BUILTIN_MALLOC is not set
CONFIG_LV_USE_CLIB_MALLOC=y
```

### 2. WiFi/LWIP 优先使用 PSRAM

目的：

- 减少 WiFi/LWIP 对 internal RAM 的压力。

当前状态：

```text
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```

对应 SDKCONFIG 调整：

```text
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```

### 3. LCD 先于 WiFi 保留关键资源

目的：

- 让 ILI9488 DMA 转换缓冲、SPI IO、panel 对象优先拿到连续 internal/DMA memory。
- 避免 WiFi 先初始化后造成 internal heap 碎片。

当前方向：

```text
LCD hardware init
LVGL port/display init
WiFi init
UI setup
network task start
```

### 4. LVGL draw buffer 放 PSRAM

目的：

- LVGL draw buffer 不占用 DMA/internal。
- internal/DMA 留给 WiFi、SPI driver、ILI9488 conversion buffer。

注意：

- LVGL draw buffer 放 PSRAM不等于 LCD 完全不需要 DMA。
- ILI9488 conversion buffer 仍需要 DMA-capable memory。

## SDKCONFIG 配置修改总结

本项目的 `sdkconfig` 不直接手改，所有配置通过 `idf.py menuconfig` 调整。本节只记录已经排查确认过的配置方向。

### 必须保留的配置

#### 1. LVGL 内存分配器改为 C malloc

配置项：

```text
# CONFIG_LV_USE_BUILTIN_MALLOC is not set
CONFIG_LV_USE_CLIB_MALLOC=y
```

原因：

- LVGL built-in malloc 会创建一块静态内存池。
- 之前 128KB LVGL 静态池会长期占用内部 RAM。
- UI + WiFi + LCD 合并后，内部 RAM 和 DMA-capable RAM 都很紧张。
- 改为 C malloc 后，LVGL 对象/资源可以走系统 heap 策略，减少固定 internal RAM 占用。

影响：

- 明显释放 internal RAM。
- 当前日志中启动初期 internal RAM 从约 166KB 提升到约 296KB，说明该配置有效。
- 这是解决组合运行问题的基础配置，不建议回退。

#### 2. WiFi/LWIP 优先使用 PSRAM

配置项：

```text
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```

原因：

- WiFi/LWIP 会消耗大量 internal RAM。
- LCD ILI9488 18-bit SPI 路径还需要 DMA-capable RGB565 -> RGB666 转换缓冲。
- 让 WiFi/LWIP 尽量使用 PSRAM，可以减少它和 LCD/LVGL 对 internal RAM 的竞争。

影响：

- 降低 WiFi 初始化后 internal/DMA heap 的压力。
- 不能完全消除 WiFi internal RAM 使用，因为 WiFi 驱动仍有部分结构必须在 internal RAM。
- 仍需要配合初始化顺序和 LCD/LVGL buffer 策略。

#### 3. PSRAM heap 必须保持启用

配置方向：

```text
CONFIG_SPIRAM_USE_MALLOC=y
```

原因：

- LVGL draw buffer 当前设计为放到 PSRAM。
- WiFi/LWIP 也需要有机会把可外置的 buffer 放到 PSRAM。
- 如果 PSRAM 没有加入 malloc heap，`MALLOC_CAP_SPIRAM` 分配会失败。

影响：

- 保证 LVGL draw buffer 不挤占 internal/DMA RAM。
- 让 `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` 有实际意义。

### 建议确认的配置

#### 1. FreeRTOS 栈溢出检测

建议方向：

```text
CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y
```

用途：

- 当前历史日志出现过 corrupted backtrace、`0xa5a5a5a5`、`0xf4f4f4f4`。
- 这些现象可能来自栈溢出、heap 越界、use-after-free 或 DMA/缓冲越界。
- 栈 canary 可以更早暴露 task stack 被破坏的问题。

影响：

- 有少量性能/内存开销。
- 适合排查阶段开启。

#### 2. Heap poisoning

建议方向：

```text
CONFIG_HEAP_POISONING_LIGHT=y
```

用途：

- 用于捕获 heap 越界写、use-after-free 等问题。
- 对当前 “可能不是现场立即崩溃，而是后续 WDT/LoadProhibited” 的问题很有帮助。

影响：

- 有性能开销。
- 适合调试阶段开启；最终发布可视情况关闭。

#### 3. LVGL assert

当前已看到的相关配置：

```text
CONFIG_LV_USE_ASSERT_NULL=y
CONFIG_LV_USE_ASSERT_MALLOC=y
```

建议排查阶段可考虑：

```text
CONFIG_LV_USE_ASSERT_MEM_INTEGRITY=y
CONFIG_LV_USE_ASSERT_OBJ=y
```

用途：

- 更早发现 LVGL 对象非法、内存破坏、错误对象生命周期。
- 对切屏到 Standby 后崩溃的问题尤其有帮助。

影响：

- 会增加运行时开销。
- 如果开启后更早 assert，是好事，说明问题被定位到更接近源头的位置。

### 可选优化配置

#### 1. 降低 WiFi buffer 数量

只有在以下情况仍存在时再考虑：

- LCD/LVGL 已稳定。
- LVGL display attach 已稳定。
- 仍然在 WiFi 初始化后出现 internal/DMA 连续内存不足。

可考虑方向：

```text
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM
```

原则：

- 不要一开始就大幅降低。
- 先保留默认值验证稳定性。
- 如果仍然内存紧张，再小步下调。

影响：

- 可以降低 WiFi 内存占用。
- 可能降低 WiFi 吞吐、弱网稳定性或并发能力。
- 当前问题还没有证据指向必须先调 WiFi buffer。

### 不建议作为解决方案的配置方向

#### 1. 试图让 LCD 完全不使用 DMA

原因：

- 当前 ILI9488 18-bit SPI 驱动需要 RGB565 -> RGB666 转换缓冲。
- 转换后的数据通过 `esp_lcd_panel_io_tx_color()` 发送。
- 该转换缓冲需要 DMA-capable memory。

结论：

- LVGL draw buffer 可以不用 DMA，可以放 PSRAM。
- ILI9488 panel conversion buffer 仍然需要 DMA。
- 所以不能把“关闭 LCD DMA”当作根本方案。

#### 2. 只增大 WDT 超时时间

原因：

- 当前 WDT 是结果，不是根因。
- 如果底层是内存破坏、SPI/LVGL 时序卡死、或转换缓冲越界，增大 WDT 只会延迟复位。

结论：

- 不建议用 WDT 超时配置掩盖问题。
- 应先定位卡死/越界/锁时序。

## 尚未最终排除的原因

### 1. ILI9488 转换缓冲越界

表现可能是：

- FreeRTOS task list 损坏。
- `LoadProhibited`。
- `A2 = 0xa5a5a5a5` 或 `A11 = 0xf4f4f4f4`。
- backtrace corrupted。
- 后续 WDT。

这类问题不一定在写越界现场立刻崩溃。

### 2. LVGL task/lock 时序

风险点：

- `app_main()` 创建 UI 时必须持有 `lvgl_port_lock()`。
- event 回调中切屏要确认是在 LVGL 上下文内，或者使用 `lv_async_call()`。
- 非 LVGL task 不能直接调用 LVGL API。

### 3. Standby screen 创建/切屏路径

待基础显示稳定后继续检查：

- `components/ui_custom/events_init.c`
- `components/ui_custom/setup_scr_screen_Standby.c`
- 是否切屏时重复创建对象。
- 是否存在旧 screen 已删除但指针仍被使用。
- 是否有全屏 invalidate 触发超过 LCD conversion buffer 的 flush。

### 4. 栈/堆破坏

历史日志曾出现：

```text
Guru Meditation Error: Core 0 panic'ed (LoadProhibited)
Backtrace ... |<-CORRUPTED
A2 = 0xa5a5a5a5
A11 = 0xf4f4f4f4
```

这通常指向：

- use-after-free
- 栈溢出
- heap 越界写
- 驱动 DMA/缓冲越界

当前更偏向缓冲越界或异步 LCD/LVGL 时序，而不是普通 C 空指针。

## 后续验证清单

下一次运行重点看这些日志是否出现：

```text
before app_lvgl_add_spi_display
after app_lvgl_add_spi_display
before lvgl_port_add_touch
after lvgl_port_add_touch
LVGL init done
```

如果已经进入 UI，再继续看：

```text
post-LVGL WiFi init begin
post-LVGL WiFi init done
network_started
Return 点击坐标
setup_scr_screen_Standby begin
```

如果再次复位，应优先补充：

- 首帧 flush area 日志。
- 切屏 flush area 日志。
- 每次 flush 的 pixel count。
- ILI9488 conversion buffer capacity。
- LVGL API 调用时是否持锁。

## 操作约束

后续排查遵守：

- 不直接编辑 `sdkconfig`。
- `sdkconfig` 相关修改由 menuconfig 完成。
- 不直接运行 `idf.py`。
- 只列出需要执行的 `idf.py` 命令，由使用者自行运行。
