# PROJECT_ARCHITECTURE — 项目全局架构与开发指南

> **目的**：本文档是面向 AI 辅助开发的"上下文注入文件"。在新会话中只需读取本文件，即可全面了解项目的架构、规范和当前状态，无需逐个翻阅源码。
>
> **最后更新**：2026-03-17

---

## 1. 项目概要

| 项目         | 说明                                                                 |
|--------------|----------------------------------------------------------------------|
| **名称**     | 基于物联网的企业培训管理平台 — 智能终端固件 (Smart Training Terminal) |
| **目标芯片** | Espressif **ESP32-S3** (双核 Xtensa LX7)                            |
| **框架**     | **ESP-IDF v5.4.3**                                                   |
| **语言**     | C (C11)                                                              |
| **构建系统** | CMake (ESP-IDF 内置，project name: `demo`)                           |
| **许可证**   | Apache-2.0                                                           |

**一句话描述**：终端负责现场人员身份核验、环境感知（温湿度/CO2）及数据上报（MQTT），围绕"签到—学习—考核—评价"全流程构建数据闭环。

---

## 2. 技术栈概览

| 层次         | 技术 / 组件                                          |
|--------------|------------------------------------------------------|
| RTOS         | FreeRTOS (ESP-IDF 内置，双核 SMP)                    |
| 网络协议     | WiFi STA (WPA2/WPA3) · MQTT/TLS (规划中)            |
| 传感器驱动   | DHT11/DHT22/Si7021 (GPIO 单总线) · CO2 (规划中)     |
| 身份核验     | 人脸识别模组 / 指纹 / NFC-RFID (全部规划中)          |
| 本地存储     | NVS (已实现) · SD 卡离线缓存 (规划中)                |
| 外部依赖     | `esp-idf-lib/dht v1.2.0` (Espressif 组件仓库)       |
| 配置管理     | `Kconfig.projbuild` (menuconfig 可视化配置)          |
| 版本控制     | Git, Conventional Commits                            |

---

## 3. 目录结构说明

```text
demo/                                  ← 项目根目录
├── CMakeLists.txt                     ← 顶层构建脚本 (project(demo))
├── sdkconfig                          ← 当前芯片/功能配置 (menuconfig 生成，不应提交)
├── dependencies.lock                  ← ESP-IDF 组件管理器锁文件
├── ESP32_Development_Standards.md     ← 完整开发规范手册 (编码/Git/流程)
├── PROJECT_ARCHITECTURE.md            ← 【本文档】架构速览
├── README.md                          ← 项目功能介绍、硬件架构、快速开始
│
├── main/                              ═══ 应用层 (业务逻辑 + 任务调度) ═══
│   ├── CMakeLists.txt                 ← 注册源文件: main.c, app_sensor.c, app_network.c
│   ├── Kconfig.projbuild              ← menuconfig 配置项 (WiFi SSID/密码/重试次数)
│   ├── main.c                         ← app_main() 系统入口，仅做初始化和任务创建
│   ├── app_sensor.c                   ← 传感器采集任务封装 (sensor_task)
│   ├── app_network.c                  ← 网络管理任务封装 (network_task)
│   └── include/
│       ├── app_sensor.h               ← API: app_sensor_start()
│       └── app_network.h              ← API: app_network_start()
│
├── components/                        ═══ 可复用组件层 (与业务逻辑解耦) ═══
│   ├── app_wifi/                      ← WiFi STA 连接管理组件
│   │   ├── CMakeLists.txt             ←   REQUIRES: esp_wifi, nvs_flash, esp_event, driver
│   │   ├── app_wifi.c                 ←   事件驱动连接 + 自动重试 + EventGroup 同步
│   │   └── include/
│   │       └── app_wifi.h             ←   API: app_wifi_init(), app_wifi_wait_connected()
│   │
│   ├── drivers/                       ← 外部设备驱动 (非 ESP32 内部外设)
│   │   ├── CMakeLists.txt             ←   REQUIRES: driver, esp_timer
│   │   ├── driver_dht.c              ←   DHT11/22/Si7021 温湿度驱动 (临界区时序保护)
│   │   └── include/
│   │       └── driver_dht.h           ←   API: driver_dht_read_data(), driver_dht_read_float_data()
│   │
│   ├── bsp/                           ← 板级支持包 — 仅有骨架，暂无源文件
│   │   ├── CMakeLists.txt             ←   (SRCS 为空)
│   │   └── README.md                  ←   规划: bsp_pin_defs.h, bsp_i2c, bsp_spi, bsp_power
│   │
│   └── common/                        ← 通用工具库 — 仅有骨架，暂无源文件
│       ├── CMakeLists.txt             ←   (SRCS 为空)
│       └── README.md                  ←   规划: 字符串工具、数学辅助、通用数据结构封装
│
├── managed_components/                ← ESP-IDF 组件管理器自动下载的第三方库
├── docs/                              ← 项目文档
└── tools/                             ← 辅助脚本
```

### `main/` 与 `components/` 的职责区分

| 维度     | `main/` 应用层                     | `components/` 组件层                 |
|----------|------------------------------------|--------------------------------------|
| 职责     | 业务逻辑、任务编排、系统初始化     | 可复用的功能模块，与具体业务无关     |
| 耦合     | 可依赖任意组件                     | 组件间禁止互相依赖（除显式 REQUIRES）|
| 头文件   | `main/include/`                    | `组件名/include/`                    |
| 示例     | `app_sensor.c` 调用 `driver_dht.h` | `driver_dht.c` 只依赖 ESP-IDF API   |

---

## 4. 架构设计

### 4.1 分层架构

```
┌───────────────────────────────────────────────────────────┐
│                     应用层 (main/)                         │
│   main.c          → 系统入口，NVS 初始化，任务创建         │
│   app_sensor.c    → 传感器采集任务，调用 drivers 组件       │
│   app_network.c   → 网络管理任务，调用 app_wifi 组件        │
├───────────────────────────────────────────────────────────┤
│                   组件层 (components/)                      │
│   app_wifi/       → WiFi 连接维护 (事件驱动，自动重连)      │
│   drivers/        → 底层外设驱动 (DHT11 温湿度)             │
│   bsp/            → 板级硬件抽象 (引脚/总线)  ← 预留        │
│   common/         → 通用工具库                ← 预留        │
├───────────────────────────────────────────────────────────┤
│              ESP-IDF 框架 + FreeRTOS                       │
│   esp_wifi, esp_event, nvs_flash, driver, esp_timer ...   │
└───────────────────────────────────────────────────────────┘
```

**依赖方向**：应用层 → 组件层 → ESP-IDF，严禁反向依赖。

### 4.2 FreeRTOS 任务架构

```
app_main() ─────────────────────────────────────────────────
  │  1. nvs_flash_init()           ← 系统级初始化
  │  2. app_sensor_start()         ← 创建并发任务
  │  3. app_network_start()        ← 创建并发任务 (当前注释中)
  │  4. 函数返回 → 任务自动删除
  │
  ├──► sensor_task (栈:2048B, 优先级:5)
  │      while(1):
  │        driver_dht_read_float_data(DHT11, GPIO4, &h, &t)
  │        ESP_LOGI → 打印温湿度
  │        TODO: 将数据发送到队列
  │        vTaskDelay(2000ms)
  │
  └──► network_task (栈:4096B, 优先级:5)
         app_wifi_init()
         while(1):
           app_wifi_wait_connected()  ← 阻塞等待
           TODO: app_mqtt_start()
           vTaskDelay(5000ms)
```

### 4.3 调用关系图

```
main.c
  ├── nvs_flash_init()                            ← ESP-IDF
  │
  ├── app_sensor_start() ──────► app_sensor.c
  │                                 └── driver_dht_read_float_data() ──► components/drivers/driver_dht.c
  │                                                                         └── GPIO 单总线时序操作
  │
  └── app_network_start() ─────► app_network.c
                                    ├── app_wifi_init() ───────────────► components/app_wifi/app_wifi.c
                                    │                                      ├── esp_wifi_init/start/connect
                                    │                                      └── event_handler (事件回调)
                                    └── app_wifi_wait_connected() ─────► xEventGroupWaitBits()
```

### 4.4 事件驱动模型 (WiFi 示例)

```
ESP-IDF 默认事件循环
    │
    ├─ WIFI_EVENT_STA_START        → esp_wifi_connect()           发起连接
    ├─ WIFI_EVENT_STA_DISCONNECTED → 重试 (s_retry_num < MAX)    自动重连
    │                                 或置位 WIFI_FAIL_BIT        达到上限
    └─ IP_EVENT_STA_GOT_IP        → 置位 WIFI_CONNECTED_BIT     连接成功
                                     s_retry_num = 0              重置计数

network_task 通过 xEventGroupWaitBits(CONNECTED | FAIL) 阻塞等待连接结果
```

---

## 5. 组件间通信规则

```
✅ 推荐的通信方式：
   • FreeRTOS EventGroup  → 状态标志同步 (如: WiFi 连接成功/失败)
   • FreeRTOS Queue       → 数据流传递 (如: 传感器数据 → 网络发送)
   • ESP-IDF Event Loop   → 事件驱动解耦 (如: WiFi/IP 事件)
   • Callback 函数        → 一对一通知

❌ 禁止的通信方式：
   • 全局变量共享
   • 组件 A 直接 #include 组件 B 的 .c 源文件
   • 直接访问其他模块的 static 变量
```

---

## 6. 配置管理

通过 `idf.py menuconfig` → "Example Configuration" 可配置：

| 配置项             | Kconfig 宏                  | 默认值       |
|--------------------|-----------------------------|-------------|
| WiFi SSID          | `CONFIG_ESP_WIFI_SSID`      | `myssid`    |
| WiFi 密码          | `CONFIG_ESP_WIFI_PASSWORD`  | `mypassword`|
| 最大重试次数       | `CONFIG_ESP_MAXIMUM_RETRY`  | `5`         |
| WPA3 SAE 模式      | `CONFIG_ESP_WPA3_SAE_PWE_*` | `BOTH`      |
| WiFi 认证模式阈值  | `CONFIG_ESP_WIFI_AUTH_*`    | `WPA2_PSK`  |

硬件引脚当前硬编码在源文件中（规划迁移至 BSP）：

| 外设                | GPIO     | 定义位置             |
|---------------------|----------|----------------------|
| DHT11 温湿度传感器  | GPIO_NUM_4 | `app_sensor.c:12`  |

---

## 7. 当前实现状态

| 模块              | 状态       | 关键文件                         | 说明                         |
|-------------------|------------|----------------------------------|------------------------------|
| 系统入口 / NVS    | ✅ 完成    | `main/main.c`                    | NVS 初始化，含擦除重建逻辑   |
| WiFi STA 连接     | ✅ 完成    | `components/app_wifi/`           | 事件驱动，自动重连，Kconfig  |
| DHT11 温湿度驱动  | ✅ 完成    | `components/drivers/driver_dht.c`| 支持 DHT11/22/Si7021，临界区 |
| 传感器采集任务    | ✅ 完成    | `main/app_sensor.c`             | 2s 周期采集，GPIO4           |
| 网络管理任务      | ✅ 完成    | `main/app_network.c`            | 框架就绪，main.c 中调用已注释|
| MQTT 通信         | ⬜ 未开始  | —                               | network_task 中预留 TODO     |
| 传感器数据队列    | ⬜ 未开始  | —                               | sensor_task 中预留 TODO      |
| BSP 板级支持      | ⬜ 骨架    | `components/bsp/`               | 仅有 README，无源码          |
| 通用工具库        | ⬜ 骨架    | `components/common/`            | 仅有 README，无源码          |
| 身份核验          | ⬜ 未开始  | —                               | README 中规划                |
| SD 卡离线缓存     | ⬜ 未开始  | —                               | README 中规划                |
| OTA 远程升级      | ⬜ 未开始  | —                               | README 中规划                |
| UI 交互           | ⬜ 未开始  | —                               | main.c 中预留注释            |

---

## 8. 核心开发规范

> 完整规范见 `ESP32_Development_Standards.md`，以下为关键摘要。

### 8.1 命名约定

| 类别             | 规则                          | 示例                                          |
|------------------|-------------------------------|-----------------------------------------------|
| 文件名           | 小写 + 下划线                 | `app_wifi.c`, `driver_dht.c`                  |
| 全局 API 函数    | `[组件名]_[动词]_[名词]()`    | `app_wifi_init()`, `driver_dht_read_data()`   |
| 静态函数         | `static` 修饰，动词开头       | `static void event_handler()`                 |
| 静态/模块级变量  | `s_` 前缀                    | `s_wifi_event_group`, `s_retry_num`           |
| 全局变量         | `g_` 前缀 (尽量避免)         | `g_system_state`                              |
| 常量/宏          | 全大写 + 下划线               | `WIFI_CONNECTED_BIT`, `DHT_DATA_BITS`         |
| 日志 TAG         | 每个 `.c` 文件独立定义        | `static const char *TAG = "app_wifi";`        |

### 8.2 组件目录结构模板

每个 `components/` 下的组件必须遵循：

```text
components/
└── my_module/
    ├── CMakeLists.txt          ← idf_component_register(SRCS ... INCLUDE_DIRS "include" REQUIRES ...)
    ├── my_module.c             ← 实现 (内部函数用 static 修饰)
    └── include/
        └── my_module.h         ← 仅暴露必要的 API 接口 (#pragma once)
```

### 8.3 如何新增一个组件

以 `app_mqtt` 组件为完整示例：

**步骤 1 — 创建目录结构**
```text
components/app_mqtt/
├── CMakeLists.txt
├── app_mqtt.c
└── include/
    └── app_mqtt.h
```

**步骤 2 — CMakeLists.txt**
```cmake
idf_component_register(SRCS "app_mqtt.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mqtt esp_event)
```

**步骤 3 — 定义头文件接口 (app_mqtt.h)**
```c
#pragma once
#include "esp_err.h"

esp_err_t app_mqtt_init(void);
esp_err_t app_mqtt_publish(const char *topic, const char *data, int len);
```

**步骤 4 — 实现源文件 (app_mqtt.c)**
```c
#include "app_mqtt.h"
#include "esp_log.h"

static const char *TAG = "app_mqtt";

// 内部函数一律用 static 修饰
static void mqtt_event_handler(void *arg, ...) { ... }

// 对外 API 返回 esp_err_t
esp_err_t app_mqtt_init(void) {
    ESP_LOGI(TAG, "MQTT 初始化...");
    // ...
    return ESP_OK;
}
```

**步骤 5 — 在应用层调用**

在 `main/app_network.c` 中 `#include "app_mqtt.h"` 并调用。
ESP-IDF 会自动发现 `components/` 下的新组件，无需修改顶层 CMakeLists.txt。

### 8.4 如何新增一个应用层任务模块

以 `app_display` 为完整示例：

**步骤 1 — 创建文件**
- `main/app_display.c`
- `main/include/app_display.h`

**步骤 2 — 头文件 (统一接口模式)**
```c
#pragma once
#include "esp_err.h"

esp_err_t app_display_start(void);
```

**步骤 3 — 源文件 (标准任务模板)**
```c
#include "app_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "app_display";
static TaskHandle_t s_display_task_handle = NULL;

static void display_task(void *pvParameters) {
    ESP_LOGI(TAG, "显示任务启动...");
    while (1) {
        // 业务逻辑...
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t app_display_start(void) {
    if (s_display_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;  // 防止重复创建
    }
    BaseType_t ret = xTaskCreate(display_task, "display_task",
                                  4096, NULL, 5, &s_display_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建显示任务失败");
        return ESP_FAIL;
    }
    return ESP_OK;
}
```

**步骤 4 — 注册到构建系统**

编辑 `main/CMakeLists.txt`，在 SRCS 列表中追加 `"app_display.c"`：
```cmake
idf_component_register(SRCS "main.c" "app_sensor.c" "app_network.c" "app_display.c"
                    INCLUDE_DIRS "." "include")
```

**步骤 5 — 在 main.c 中启动**
```c
#include "app_display.h"
// 在 app_main() 中：
ESP_ERROR_CHECK(app_display_start());
```

### 8.5 错误处理规范

```c
// ✅ 初始化阶段 — 失败即终止系统，使用 ESP_ERROR_CHECK
ESP_ERROR_CHECK(esp_wifi_init(&cfg));

// ✅ 运行时 — 手动检查返回值，记录日志，向上传递错误码
esp_err_t ret = some_operation();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "操作失败: %s", esp_err_to_name(ret));
    return ret;
}

// ❌ 禁止：忽略返回值
esp_wifi_connect();  // 不检查返回值 — 禁止!
```

- 所有对外 API 函数应返回 `esp_err_t`
- `void` 返回类型仅用于内部确定不会失败的操作

### 8.6 日志级别

| 级别       | 宏           | 使用场景                                |
|------------|-------------|----------------------------------------|
| 错误       | `ESP_LOGE`  | 系统无法继续某项功能                     |
| 警告       | `ESP_LOGW`  | 异常但可恢复 (如传感器单次读取失败)      |
| 信息       | `ESP_LOGI`  | 关键流程节点 (启动、连接成功、任务创建)  |
| 调试       | `ESP_LOGD`  | 详细数据内容，发布版本时应关闭           |

### 8.7 FreeRTOS 使用要点

- `app_main()` 不写死循环，用完即返回，任务自动删除
- 任务栈大小通过 `uxTaskGetStackHighWaterMark()` 调试确定，避免浪费
- 优先使用 `EventGroup` 处理状态标志同步
- 优先使用 `Queue` 处理数据流传递
- 慎用 `Semaphore` 互斥锁，尽量通过架构设计避免资源竞争

---

## 9. 关键接口速查

### components/app_wifi

```c
void      app_wifi_init(void);            // 初始化 WiFi STA，注册事件处理，发起连接
esp_err_t app_wifi_wait_connected(void);  // 阻塞等待连接成功(ESP_OK)或失败(ESP_FAIL)
```

### components/drivers (driver_dht)

```c
typedef enum {
    DRIVER_DHT_TYPE_DHT11,     // DHT11
    DRIVER_DHT_TYPE_AM2301,    // AM2301 (DHT21, DHT22, AM2302, AM2321)
    DRIVER_DHT_TYPE_SI7021     // Itead Si7021
} driver_dht_type_t;

// 读取整数数据 (返回值 = 实际值 × 10, 如 humidity=625 表示 62.5%)
esp_err_t driver_dht_read_data(driver_dht_type_t type, gpio_num_t pin,
                                int16_t *humidity, int16_t *temperature);

// 读取浮点数据 (直接返回实际值)
esp_err_t driver_dht_read_float_data(driver_dht_type_t type, gpio_num_t pin,
                                      float *humidity, float *temperature);
```

### main/app_sensor

```c
esp_err_t app_sensor_start(void);  // 创建 sensor_task, 2s 周期采集 DHT11(GPIO4)
```

### main/app_network

```c
esp_err_t app_network_start(void);  // 创建 network_task, 管理 WiFi + MQTT(规划中)
```

---

## 10. Git 规范摘要

### 分支策略

| 分支            | 用途                      |
|-----------------|---------------------------|
| `main`          | 稳定发布分支，随时可发布  |
| `develop`       | 开发主分支                |
| `feature/xxx`   | 功能分支，从 develop 检出 |
| `fix/xxx`       | 非紧急 bug 修复           |
| `hotfix/xxx`    | 紧急修复，从 main 检出    |

### 提交信息格式

```
<type>(<scope>): <subject>
```

| type       | 含义             |
|------------|------------------|
| `feat`     | 新功能           |
| `fix`      | Bug 修复         |
| `docs`     | 文档变更         |
| `refactor` | 重构             |
| `style`    | 格式调整         |
| `perf`     | 性能优化         |
| `test`     | 测试             |
| `chore`    | 构建/辅助工具    |

**scope 示例**: `wifi`, `sensor`, `main`, `bsp`, `mqtt`

### 合并与发布

- 禁止直接 Push 到 `main` 或 `develop`，所有变更通过 PR 合并
- 推荐 **Squash and Merge** 保持主分支历史整洁
- 发布时在 `main` 打 Tag: `v<Major>.<Minor>.<Patch>`

---

## 11. 构建与烧录

```bash
# 配置 (WiFi SSID/密码等通过 menuconfig 设置)
idf.py menuconfig

# 编译
idf.py build

# 烧录并监控串口
idf.py -p PORT flash monitor
```

---

## 12. 架构设计原则 (总结)

1. **分层隔离**：`main/`(应用) → `components/`(组件) → ESP-IDF(框架)，单向依赖，禁止反向调用
2. **高内聚低耦合**：每个组件只做一件事，通过队列/事件/回调通信，禁止全局变量
3. **接口先行**：先写 `.h` 定义 API，再写 `.c` 实现；对外 API 统一返回 `esp_err_t`
4. **私有化实现**：内部函数和变量一律 `static` 修饰，头文件仅暴露必要接口
5. **任务独立**：每个业务模块拥有独立 FreeRTOS 任务，通过 `xxx_start()` 统一入口启动
6. **防御性编程**：所有可失败调用必须检查返回值，驱动层使用 `CHECK_ARG` 校验参数
7. **可配置化**：运行参数通过 Kconfig 管理，硬件引脚规划迁移至 BSP 统一定义
