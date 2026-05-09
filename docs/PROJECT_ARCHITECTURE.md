# PROJECT_ARCHITECTURE - 项目全局架构与开发指南

> 本文档面向 AI 辅助开发和后续维护。新会话开始时应先阅读本文件，再阅读 `main/app_main()` 调用链和相关组件源码。
>
> 最后更新：2026-05-08

---

## 1. 项目概要

| 项目 | 说明 |
| --- | --- |
| 名称 | 基于物联网的企业培训管理平台 - 智能终端固件 |
| 芯片 | ESP32-S3 |
| 框架 | ESP-IDF v5.4.3 |
| 语言 | C |
| 图形 | LVGL 9.x + GUI Guider 生成界面 |
| 显示 | ILI9488 SPI LCD，逻辑横屏 480x320 |
| 触摸 | XPT2046 SPI Touch |
| 网络 | WiFi STA + MQTT/OneNET |
| 身份核验 | RFID/IC 卡 + FM225 人脸模块 + AS608 指纹模块 |
| 本地存储 | NVS |

一句话描述：终端用于企业线下培训现场的人员身份核验、考勤记录、环境数据采集和 MQTT 上报，目标业务闭环是“课前发卡/建档 -> 首次注册生物特征 -> 日常刷卡核验 -> 本地记录 -> 云端上报”。

---

## 2. 当前工程状态

当前代码已经不是早期骨架状态，已包含 UI、BSP、WiFi、MQTT、RFID、人脸、指纹、传感器和性能监控模块。

| 模块 | 状态 | 关键文件 | 说明 |
| --- | --- | --- | --- |
| 系统入口 | 已接入 | `main/main.c` | 初始化 NVS、LVGL、GUI Guider UI、网络和性能监控 |
| LVGL 显示 | 已接入 | `main/app_lvgl.c`, `components/bsp/bsp_lcd.c` | ILI9488 + esp_lvgl_port，LVGL 任务固定 Core 1 |
| GUI Guider UI | 已移植 | `components/ui_custom/` | 包含自检页和 7 个业务页面 |
| UI 事件 | 部分接入 | `components/ui_custom/events_init.c` | 目前只接入自检页 -> Standby 的跳转 |
| BSP LCD/Touch | 已接入 | `components/bsp/` | SPI LCD 与 XPT2046 触摸共用 SPI2 |
| WiFi | 已接入 | `components/app_wifi/` | STA 连接、重试、EventGroup 同步 |
| MQTT | 已接入 | `components/app_mqtt/`, `main/app_network.c` | 当前上报传感器属性；考勤事件上报待接入 |
| 时间同步 | 已接入 | `main/app_time.c` | WiFi 后 SNTP 单次对时 |
| 传感器队列 | 已接入 | `main/sensor_data.c`, `components/common/data_queue.c` | 传感器数据通过队列给网络任务 |
| DHT11 | 已接入 | `main/app_sensor.c`, `components/drivers/driver_dht.c` | GPIO6，当前 `app_main` 未启动 |
| CO2 | 已接入 | `main/app_co2.c`, `components/drivers/driver_co2.c` | UART0 GPIO4/5，当前 `app_main` 未启动；启用时需释放 UART0 console |
| RFID | 驱动完成，业务未接 | `main/app_rfid.c` | 支持 RC522 I2C、读写块、刷卡回调；当前 `app_main` 未启动 |
| 人脸 | 驱动封装完成，业务未接 | `main/app_face.c`, `components/drivers/driver_fm225.c` | 支持注册和单次识别 |
| 指纹 | 驱动封装完成，业务未接 | `main/app_fingerprint.c`, `components/drivers/driver_as608.c` | 支持注册、识别、删除、清空 |
| 学员档案 | 模块完成，业务未初始化 | `main/attendance_profile.c` | NVS 中保存学号、卡标识、人脸 ID、指纹页号映射 |
| 性能监控 | 已接入 | `main/app_perf_monitor.c` | 输出任务运行时间、CPU 占用、栈水位，关注 WiFi/LVGL/UI |

重要现状：UI 页面和底层能力已经存在，但完整的“刷卡 -> 注册/验证 -> 成功/失败 -> MQTT 考勤事件上报”业务状态机尚未在代码中闭环。

---

## 3. 目录结构

```text
demo/
├── main/                         应用层：业务编排、任务创建、状态机
│   ├── main.c                    app_main() 系统入口
│   ├── app_lvgl.c                LVGL port + LCD/touch 接入
│   ├── app_network.c             WiFi、SNTP、MQTT、传感器数据上报
│   ├── app_rfid.c                RFID/RC522 应用封装
│   ├── app_face.c                FM225 人脸模块应用封装
│   ├── app_fingerprint.c         AS608 指纹模块应用封装
│   ├── attendance_profile.c      学员身份映射表，NVS 持久化
│   ├── sensor_data.c             传感器队列
│   └── include/                  应用层头文件
├── components/
│   ├── ui_custom/                GUI Guider 生成 UI
│   ├── bsp/                      LCD、Touch、板级引脚
│   ├── app_wifi/                 WiFi STA 组件
│   ├── app_mqtt/                 MQTT 组件
│   ├── common/                   通用队列封装
│   └── drivers/                  DHT、CO2、AS608、FM225 等底层驱动
├── docs/
│   ├── PROJECT_ARCHITECTURE.md
│   ├── WIFI_LCD_LVGL_RESET_INVESTIGATION.md
│   └── attendance_bio_flow.drawio
├── sdkconfig
└── partitions.csv
```

依赖方向必须保持单向：

```text
main/ 应用层 -> components/ 组件层 -> ESP-IDF/第三方驱动
```

组件层不要反向依赖 `main/`。业务状态机放在 `main/`，可复用硬件能力放在 `components/` 或 `drivers/`。

---

## 4. app_main 当前启动链路

`main/main.c` 当前实际执行流程：

```text
app_main()
  ├── nvs_flash_init()
  ├── log_system_status("nvs_inited")
  ├── app_lvgl_init()
  │   ├── bsp_lcd_init()
  │   │   ├── backlight_init()
  │   │   ├── spi_bus_initialize(SPI2_HOST)
  │   │   ├── esp_lcd_new_panel_io_spi()
  │   │   └── esp_lcd_new_panel_ili9488()
  │   ├── bsp_touch_init()
  │   ├── lvgl_port_init()
  │   ├── lvgl_port_add_disp()
  │   ├── lvgl_port_add_touch()
  │   └── bsp_lcd_backlight_set(80)
  ├── lvgl_port_lock()
  │   ├── setup_ui(&guider_ui)
  │   │   ├── init_scr_del_flag()
  │   │   ├── setup_scr_screen()
  │   │   └── lv_screen_load(自检页)
  │   └── events_init(&guider_ui)
  ├── events_selfcheck_set_result(...)
  ├── events_selfcheck_finish()
  │   └── 通过 LVGL timer/async 延迟进入 Standby
  ├── app_network_start()
  │   └── network_task(Core 0)
  │       ├── app_wifi_init()
  │       ├── app_wifi_wait_connected()
  │       ├── app_time_sync_once()
  │       ├── app_mqtt_start()
  │       └── 循环读取 sensor_queue 并上报属性
  ├── app_perf_monitor_start()
  └── app_main 返回
```

当前被注释或尚未启动的关键业务能力：

- `attendance_profile_init()` 未在 `app_main()` 调用。
- `app_rfid_start()` 未启动，`app_rfid_set_card_callback()` 未绑定业务回调。
- FM225/AS608 自检和初始化代码在 `main.c` 中大段注释。
- UI 的 Confirm/face/finger/success/unregistered/admin 页面按钮没有业务事件绑定。
- MQTT 当前主要上报环境属性，考勤事件上报尚未接入。

---

## 5. GUI Guider UI 页面

`components/ui_custom/gui_guider.h` 中实际包含 8 个 screen，其中 1 个启动自检页，7 个业务页。

| 页面 | 代码对象 | 角色 |
| --- | --- | --- |
| 自检页 | `screen` | 启动时显示 RFID/Face/Finger/Network 自检状态 |
| 待机页 | `screen_Standby` | 日常等待刷卡，显示时间、设备状态、最近结果 |
| 确认页 | `screen_Confirm` | 刷卡后显示学号/卡信息，让学员选择人脸或指纹验证 |
| 人脸页 | `screen_face` | 人脸验证进行中，显示学号和超时提示 |
| 指纹页 | `screen_finger` | 指纹验证进行中，显示学号和超时提示 |
| 成功页 | `screen_success` | 打卡成功，显示学号、卡号、时间 |
| 未注册/失败页 | `screen_unregistered` | 卡未注册、未绑定、验证失败或非本人 |
| 管理员页 | `screen_admin` | 管理员/注册流程，选择注册人脸或指纹 |

当前 UI 事件接入情况：

- 已接入：自检页返回按钮、自检结束后延迟进入 Standby。
- 未接入：Confirm 上的人脸/指纹按钮、各业务页返回按钮、admin 上的人脸/指纹注册按钮。

---

## 6. 业务闭环设计

### 6.1 你的业务设想是否合理

结论：作为毕设和企业线下培训终端原型，你的想法是合理的。用“学号/学员卡片 ID”作为业务唯一标识，比直接上传卡片物理 UID 更直观，也方便 MQTT 上报和平台展示。

但需要区分两个概念：

- 物理 UID：卡片芯片自身 UID，普通卡通常不可改，特殊 UID 卡可改。
- 卡内学号：写入 MIFARE 数据块的学号字符串，是业务标识。

建议工程上以后统一叫 `student_id` 或 `card_student_id`，不要把写进卡里的学号继续叫 `uid`。毕设可以不展开 ID 卡克隆问题，但字段命名要清楚，否则后面 RFID 驱动、档案表、MQTT 字段会混乱。

### 6.2 管理员卡分流规则

Standby 界面只做一件事：等待刷卡。系统通过卡片类型决定进入注册流程还是考勤流程。

| 刷到的卡 | 系统动作 |
| --- | --- |
| 管理员卡 | 进入 `screen_admin`，开启注册模式 |
| 学员卡，存在于 CSV/本地名单且已绑定至少一种生物特征 | 进入 `screen_Confirm`，选择人脸或指纹打卡 |
| 学员卡，存在于 CSV/本地名单但未绑定生物特征 | 进入 `screen_unregistered`，提示“未注册” |
| 未出现在 CSV/本地名单 | 进入 `screen_unregistered`，提示“未知卡片” |

管理员卡不参与考勤，只负责把系统从 Standby 切到注册模式。注册模式下，管理员再刷学员卡，系统读取学员卡片 ID，检查它是否在 CSV/本地名单中，然后允许注册人脸或指纹。

### 6.3 推荐数据模型

当前 `attendance_profile_t` 思路仍然有用，不建议删除，而是建议改名/瘦身为“学员身份映射表”。

最小字段：

| 字段 | 含义 |
| --- | --- |
| `student_id` | 学号，业务唯一主键 |
| `face_user_id` | FM225 返回的人脸用户 ID |
| `finger_page_id` | AS608 指纹库页号 |
| `has_face_bound` | 是否已绑定人脸 |
| `has_finger_bound` | 是否已绑定指纹 |

可选字段：

| 字段 | 含义 |
| --- | --- |
| `card_uid` | 物理 UID，调试或防重复用 |
| `name` | 姓名，当前可不用 |
| `updated_at` | 最近绑定时间 |

存储建议：

- 终端本地：NVS 保存映射表，适合毕设和小规模人员，当前 `attendance_profile.c` 的方式可继续用。
- 企业真实系统：服务器/后台数据库是权威数据源，终端本地只做缓存；终端离线时先本地记录，联网后补传。
- 不建议仅保存在 RAM；断电会丢失注册关系。

---

# 7. 业务流程图

```mermaid
flowchart TD
    A[课前 CSV 学员名单] --> B[批量写学员卡: student_id/card_student_id]
    B --> C[终端导入或内置名单]
    C --> D[开机自检]
    D --> E[Standby 等待刷卡]

    E --> F{刷到管理员卡?}
    F -- 是 --> G[进入 screen_admin 注册模式]
    G --> H[管理员刷学员卡]
    H --> I{学员卡是否在 CSV/名单中?}
    I -- 否 --> U1[screen_unregistered: 未知卡片]
    I -- 是 --> J[显示学员卡片 ID]
    J --> K{选择注册方式}
    K -- 人脸注册 --> L[screen_face: FM225 注册]
    K -- 指纹注册 --> M[screen_finger: AS608 注册]
    L --> N{注册成功?}
    M --> O{注册成功?}
    N -- 是 --> P[保存 student_id -> face_user_id]
    O -- 是 --> Q[保存 student_id -> finger_page_id]
    N -- 否 --> R[screen_face 修改 label_5 显示失败原因]
    O -- 否 --> S[screen_finger 修改 label_5 显示失败原因]
    P --> G
    Q --> G
    R --> G
    S --> G
    G --> T[管理员返回 Standby]

    F -- 否 --> V{卡片 ID 是否在 CSV/名单中?}
    V -- 否 --> U2[screen_unregistered: 未知卡片]
    V -- 是 --> W{是否已注册生物特征?}
    W -- 否 --> U3[screen_unregistered: 未注册]
    W -- 是 --> X[screen_Confirm: 选择打卡方式]
    X -- 人脸打卡 --> Y[screen_face: FM225 识别]
    X -- 指纹打卡 --> Z[screen_finger: AS608 识别]
    Y --> AA{返回 ID 是否匹配该卡片 ID?}
    Z --> AA
    AA -- 是 --> AB[screen_success: 本地记录考勤]
    AA -- 否 --> AC[修改当前识别页 label_5 显示失败原因]
    AB --> AD[MQTT 上报 attendance_event]
    AD --> E
    AC --> X
    U1 --> G
    U2 --> E
    U3 --> E
    T --> E
```

### 7.1 Standby 分流状态机

```text
Standby
  └── 刷卡
      ├── 管理员卡 -> screen_admin
      ├── 学员卡在 CSV/名单中
      │   ├── 已注册 -> screen_Confirm
      │   └── 未注册 -> screen_unregistered("未注册")
      └── 学员卡不在 CSV/名单中 -> screen_unregistered("未知卡片")
```

### 7.2 注册状态机

```text
screen_admin
  └── 管理员刷学员卡
      ├── 不在 CSV/名单中 -> screen_unregistered("未知卡片") -> screen_admin
      └── 在 CSV/名单中 -> 显示学员卡片 ID
          ├── 选择 Face Register -> screen_face
          │   ├── 注册成功 -> 保存 face_user_id -> 修改提示 -> screen_admin
          │   └── 注册失败 -> 修改 label_5 显示失败原因 -> screen_admin
          └── 选择 Finger Register -> screen_finger
              ├── 注册成功 -> 保存 finger_page_id -> 修改提示 -> screen_admin
              └── 注册失败 -> 修改 label_5 显示失败原因 -> screen_admin
```

### 7.3 正常考勤状态机

```text
Standby
  └── 学员刷卡且已注册 -> screen_Confirm
      ├── 选择 Face -> screen_face
      │   ├── 识别成功且 face_user_id 匹配 -> screen_success -> MQTT 上报 -> Standby
      │   └── 失败/超时/不是本人 -> 修改 label_5 -> screen_Confirm
      └── 选择 Finger -> screen_finger
          ├── 识别成功且 finger_page_id 匹配 -> screen_success -> MQTT 上报 -> Standby
          └── 失败/超时/不是本人 -> 修改 label_5 -> screen_Confirm
```

---

## 8. 真实企业线下培训通常怎么做

真实企业系统一般不会只靠终端自己维护全部名单，而是分为后台和终端两层：

1. 培训前，管理员在后台导入 CSV/Excel 名单，生成培训批次、课程、学员和权限。
2. 发卡环节把员工号/学号写入卡，或把卡号绑定到后台人员档案。
3. 首次到场时做身份采集：人脸、指纹、照片或签字，并绑定到人员档案。
4. 日常上课时，先做“卡/二维码/工号”快速定位人员，再做人脸或指纹确认本人。
5. 终端先本地落地记录，再上传服务器，避免网络波动导致考勤丢失。
6. 后台生成签到率、迟到早退、课程完成度、考试结果、评价等统计。

你的毕设可以简化为：CSV 名单 + 写卡学号 + 本地 NVS 映射 + MQTT 上报学号。这个闭环对演示足够清楚。

---

## 9. MQTT 上报建议

当前 `main/app_network.c` 上报的是 OneNET 属性主题：

```text
$sys/{pid}/{device-name}/thing/property/post
```

考勤建议使用事件模型，事件标识符：

```text
attendance_event
```

推荐字段：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `student_id` | string | 学号，主展示字段 |
| `method` | string | `face` / `fingerprint` |
| `result` | string | `ok` / `fail` |
| `reason` | string | `matched` / `not_registered` / `not_owner` / `timeout` / `module_error` |
| `face_user_id` | int | 人脸 ID，可选 |
| `finger_page_id` | int | 指纹页号，可选 |
| `ts` | int64 | Unix 毫秒时间戳 |
| `device_id` | string | 设备 ID，可选 |

最小事件示例：

```json
{
  "id": "1",
  "version": "1.0",
  "params": {
    "attendance_event": {
      "value": {
        "student_id": "202401001",
        "method": "face",
        "result": "ok",
        "reason": "matched",
        "face_user_id": 12,
        "ts": 1776001234000
      },
      "time": 1776001234000
    }
  }
}
```

---

## 10. 硬件引脚

当前 BSP 引脚定义在 `components/bsp/include/bsp_pin_defs.h`。

| 外设 | 信号 | GPIO |
| --- | --- | --- |
| LCD/Touch SPI2 | MOSI | GPIO38 |
| LCD/Touch SPI2 | MISO | GPIO41 |
| LCD/Touch SPI2 | CLK | GPIO39 |
| LCD | CS | GPIO7 |
| LCD | DC | GPIO15 |
| LCD | RST | GPIO18 |
| LCD | Backlight | GPIO40 |
| Touch | CS | GPIO42 |
| Touch | INT | NC |
| RFID RC522 I2C | SDA | GPIO21 |
| RFID RC522 I2C | SCL | GPIO47 |
| FM225 | UART1 TX/RX | GPIO11/GPIO10 |
| AS608 | UART2 TX/RX | GPIO17/GPIO16 |
| DHT11 | DATA | GPIO6 |
| CO2 | UART0 TX/RX | GPIO4/GPIO5 |

注意：

- LCD CS/DC/RST 已从高风险外部存储相关 GPIO 调整到 GPIO7/GPIO15/GPIO18。
- SPI 总线和背光仍使用 GPIO38/39/40/41/42，需结合实际 ESP32-S3 模组手册确认这些高号 GPIO 是否被 Flash/PSRAM/板载功能占用。
- UART 分配已经拆开：FM225 使用 UART1，AS608 使用 UART2，CO2 预留 UART0。
- 当前 `sdkconfig` 仍以 UART0 作为主 console；若要启用 CO2，必须通过 `idf.py menuconfig` 将 console 切到 USB Serial/JTAG，释放 UART0 给 CO2。

---

## 11. sdkconfig 与内存策略

当前 LVGL 内存策略建议：

```text
# CONFIG_LV_USE_BUILTIN_MALLOC is not set
CONFIG_LV_USE_CLIB_MALLOC=y
CONFIG_LV_USE_BUILTIN_STRING=y
CONFIG_LV_USE_BUILTIN_SPRINTF=y
```

说明：

- LVGL 使用 C library malloc 是合理的，可走 ESP-IDF heap 能力。
- 不要同时启用多个 LVGL malloc 后端。
- `sdkconfig` 应通过 `idf.py menuconfig` 维护，不建议手写修改后忘记同步。

显示缓冲注意：

- `main/app_lvgl.c` 当前 LVGL draw buffer 使用 `.buff_dma = true`，缓冲区较小，为 10 行。
- ILI9488 18bpp 转换由 panel 驱动内部转换缓冲处理。
- 后续若 UI 图片和页面继续增大，可评估 LVGL draw buffer 放 PSRAM、LCD 转换/传输缓冲保留 DMA 内存的方案。

---

## 12. 任务与核心分配

| 任务/子系统 | 推荐 Core | 当前情况 |
| --- | --- | --- |
| WiFi/MQTT/network_task | Core 0 | `app_network.c` 固定 Core 0 |
| LVGL task | Core 1 | `app_lvgl.c` `task_affinity = 1` |
| UI 业务状态机 | Core 1 或 LVGL async | 待实现 |
| RFID 扫描 | Core 1 优先 | RC522 内部任务，待业务接入后确认 |
| 人脸/指纹串口业务 | Core 1 优先 | 当前封装完成，业务未接 |
| perf_monitor | Core 1 | 已固定 Core 1 |
| DHT/CO2 | Core 1 | 任务代码已固定 Core 1，当前未启动；CO2 启用前需释放 UART0 console |

规则：

- WiFi/MQTT 尽量留在 Core 0。
- LVGL API 必须在 LVGL 上下文中调用，或通过 `lvgl_port_lock()`/`lv_async_call()` 保护。
- RFID/人脸/指纹回调不要直接长时间阻塞 LVGL 任务；建议通过队列把业务事件交给专门状态机任务。

---

## 13. 后续业务接入建议

建议新增 `main/app_attendance.c` 和 `main/include/app_attendance.h`，作为业务状态机唯一入口。

职责：

- 初始化 `attendance_profile`。
- 启动 RFID。
- 绑定刷卡回调。
- 管理当前活动学员 `student_id`。
- 识别管理员卡并切换注册模式。
- 串联注册、验证、结果页、MQTT 事件。
- 对 UI 更新统一使用 LVGL lock/async。

建议接口：

```c
esp_err_t app_attendance_start(void);
esp_err_t app_attendance_register_face(const char *student_id);
esp_err_t app_attendance_register_fingerprint(const char *student_id);
esp_err_t app_attendance_verify_face(const char *student_id);
esp_err_t app_attendance_verify_fingerprint(const char *student_id);
```

不要把业务状态机继续堆在 `main.c` 或 `components/ui_custom/events_init.c` 里。`events_init.c` 只适合绑定 UI 控件事件，把事件转发给 `app_attendance`。

---

## 14. 开发规范摘要

1. `main/` 是应用层，负责业务编排；`components/` 是可复用组件层。
2. 对外 API 优先返回 `esp_err_t`。
3. 内部函数和模块变量使用 `static`。
4. 任务使用 `xxx_start()` 统一创建，并防止重复启动。
5. 模块间通信优先使用 Queue、EventGroup、callback，避免裸全局变量互相读写。
6. `app_main()` 不写死循环，完成初始化后返回。
7. 所有可失败调用检查返回值。
8. UI 页面生成代码尽量少改；业务逻辑放在独立应用模块。
9. 硬件引脚集中在 BSP 或对应模块配置处维护，不要在多个文件散落硬编码。
10. 调试记录中已证伪的猜想不要继续当结论引用，保留为排查历史即可。

---

## 15. 已知缺口

- `attendance_profile` 当前字段仍包含 `uid` 和 `name`，与“直接读卡内学号”方案不完全贴合，建议后续重命名或新增 `student_id` 主键模型。
- UI 业务页面按钮未绑定业务事件。
- RFID 未在启动链路中启用。
- 人脸/指纹模块当前主要停留在封装和测试接口，未纳入考勤状态机。
- 考勤事件 MQTT 上报未实现。
- 本地考勤流水表尚未实现；当前只有身份映射表。
- CO2 已从 UART1 调整到 UART0；启用 CO2 前仍需把 console 从 UART0 切到 USB Serial/JTAG。
- 高号 GPIO 仍需按实际模组手册复核。
