# 基于物联网的企业培训管理平台 - 智能终端固件

这是一个运行在 ESP32-S3 上的企业线下培训考勤终端。它负责现场刷卡、人员身份核验、考勤记录、本地留档和 MQTT 上报。

当前工程已经接入 LVGL/GUI Guider 界面、RFID、FM225 人脸模块、AS608 指纹模块、WiFi、MQTT/OneNET、DHT11 环境采集和本地 SPIFFS 记录。核心业务闭环是：

```text
课前建档/发卡 -> 管理员注册人脸或指纹 -> 学员刷卡 -> 人脸/指纹确认本人 -> 本地记录 -> MQTT 上报
```

更完整的工程说明在 [docs/PROJECT_ARCHITECTURE.md](docs/PROJECT_ARCHITECTURE.md)。开发前建议先看那份文档，再看 `main/app_bootstrap.c` 和 `main/app_attendance.c`。

---

## 当前功能

- 开机自检：RFID、人脸、指纹、网络状态会在自检页显示。
- Standby 待机页：显示真实时间、WiFi 状态和环境数据，等待刷卡。
- 管理员流程：刷管理员卡进入管理员页，可选择学员并注册人脸或指纹。
- 写卡模式：管理员页可进入写卡模式，把预置学员的学号写入对应 IC 卡。
- 日常考勤：学员刷卡后进入确认页，选择人脸或指纹打卡。
- 成功记录：打卡成功后写入 `/records/attendance.csv`，成功页停留 8 秒。
- 记录查看：管理员页可进入 Records 页面，查看最近 20 条本地成功考勤记录，并支持清空。
- 云端上报：考勤成功事件通过 OneNET MQTT `thing/event/post` 上报。
- 断网补传：离线时先写本地 pending 队列，MQTT 恢复后按顺序补传。

当前只记录成功打卡流水。失败、超时、非本人等审计事件还没有写入 CSV，也没有上报到云端。

---

## 硬件与框架

| 项目 | 当前配置 |
| --- | --- |
| 主控 | ESP32-S3 |
| 框架 | ESP-IDF v5.4.3 |
| 图形 | LVGL 9.x + GUI Guider |
| 屏幕 | ILI9488 SPI LCD，逻辑横屏 480x320 |
| 触摸 | XPT2046 SPI Touch |
| 刷卡 | RC522 I2C |
| 人脸 | FM225，UART1 |
| 指纹 | AS608，UART2 |
| 网络 | WiFi STA + MQTT/OneNET |
| 本地档案 | NVS |
| 考勤流水 | `records` SPIFFS 分区 CSV |

---

## 目录结构

```text
demo/
├── main/
│   ├── main.c                    系统入口，只做 NVS 初始化和启动编排入口
│   ├── app_bootstrap.c           启动编排、自检、UI 回调桥接
│   ├── app_attendance.c          考勤业务状态机
│   ├── attendance_profile.c      学员档案和生物特征绑定，保存到 NVS
│   ├── attendance_record.c       本地考勤 CSV 和 MQTT 补传队列
│   ├── app_network.c             WiFi、SNTP、MQTT、传感器上报、考勤事件补传
│   ├── app_rfid.c                RC522 应用封装
│   ├── app_face.c                FM225 应用封装
│   ├── app_fingerprint.c         AS608 应用封装
│   └── include/                  main 层头文件
├── components/
│   ├── ui_custom/                GUI Guider 生成界面和 UI 事件桥接
│   ├── bsp/                      LCD、Touch、板级引脚
│   ├── app_wifi/                 WiFi 组件
│   ├── app_mqtt/                 MQTT 组件
│   ├── common/                   通用队列
│   └── drivers/                  DHT、CO2、FM225、AS608 等底层驱动
├── docs/
│   ├── PROJECT_ARCHITECTURE.md   当前工程结构和业务闭环说明
│   ├── attendance_profiles_seed.csv
│   └── WIFI_LCD_LVGL_RESET_INVESTIGATION.md
├── partitions.csv
└── sdkconfig
```

依赖方向保持单向：

```text
main/ 应用层 -> components/ 组件层 -> ESP-IDF/第三方驱动
```

业务状态机放在 `main/app_attendance.c`。`components/ui_custom/events_init.c` 只负责 UI 控件事件桥接，不承载业务判断。

---

## 启动链路

当前 `main.c` 已经简化，真正的启动顺序在 `app_bootstrap_start()` 里：

```text
app_main()
  ├── nvs_flash_init()
  └── app_bootstrap_start()

app_bootstrap_start()
  ├── 初始化 LVGL、LCD、Touch
  ├── 加载 GUI Guider UI
  ├── 绑定 UI 回调
  ├── 启动网络任务
  ├── 执行 RFID/FM225/AS608/Network 自检
  ├── 进入 Standby
  ├── 启动 DHT11 传感器任务
  ├── 启动考勤状态机
  └── 按调试需要启动性能监控
```

RFID 回调不会直接操作 LVGL，而是把刷卡事件投递给 `attendance_task`。所有页面跳转和控件更新都通过 UI 事件层进入 LVGL 安全上下文。

---

## 业务流程

### 管理员注册

```text
Standby
  └── 刷管理员卡 -> screen_admin
      └── 刷学员卡
          ├── 已在本地档案中 -> 选择人脸注册或指纹注册
          └── 未知卡 -> screen_unregistered，3 秒后回 Admin
```

注册成功后，系统会把 FM225 返回的人脸 ID 或 AS608 指纹页号绑定到对应学员档案。

### 日常打卡

```text
Standby
  └── 刷学员卡
      ├── 未知卡或未注册 -> screen_unregistered
      └── 已注册 -> screen_Confirm
          ├── 人脸打卡 -> ID 匹配 -> screen_success
          └── 指纹打卡 -> 页号匹配 -> screen_success
```

打卡成功后，系统会先写本地 CSV，再尝试通过 MQTT 上传。网络不可用时不会丢记录。

---

## 本地数据

| 数据 | 保存位置 | 说明 |
| --- | --- | --- |
| 学员档案 | NVS | 保存学号、卡 UID、人脸 ID、指纹页号 |
| 成功考勤流水 | `/records/attendance.csv` | SPIFFS 运行时文件，不在工程目录里 |
| 待上传队列 | `/records/attendance_upload_pending.csv` | MQTT 未确认前保留 |
| 演示名单源 | `docs/attendance_profiles_seed.csv` | 给人看的静态名单，不是设备运行时权威数据 |

考勤 CSV 表头：

```csv
ts,time,student_id,card_uid,method,result,reason,face_user_id,finger_page_id
```

设备运行日志会打印记录写入、读取、上传和补传确认状态。屏幕端可以在管理员页进入 Records 页面查看最近记录；如果要在电脑端导出完整 CSV，后续还需要加串口 dump、文件系统读取或专门导出接口。

---

## MQTT 上报

环境数据走 OneNET 属性上报，考勤成功记录走事件上报。

考勤事件 Topic：

```text
$sys/{pid}/{device-name}/thing/event/post
$sys/{pid}/{device-name}/thing/event/post/reply
```

当前事件标识符：

```text
attendance_event
```

事件字段与本地 CSV 对齐：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `ts` | int64 | Unix 秒级时间戳 |
| `time` | string | 本地时间字符串 |
| `student_id` | string | 学号 |
| `card_uid` | string | RFID 物理 UID |
| `method` | string | `face` 或 `fingerprint` |
| `result` | string | 当前成功记录为 `ok` |
| `reason` | string | 当前成功记录为 `matched` |
| `face_user_id` | int | 人脸 ID |
| `finger_page_id` | int | 指纹页号 |

补传规则很简单：打卡成功先写本地，再写 pending 队列；MQTT connected 后只发送队首一条；收到 event reply 且 `code=200` 后，才删除这条 pending 记录。

---

## 硬件引脚

当前引脚集中在 `components/bsp/include/bsp_pin_defs.h`。

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
| RFID RC522 I2C | SDA | GPIO20 |
| RFID RC522 I2C | SCL | GPIO21 |
| FM225 | UART1 TX/RX | GPIO11/GPIO10 |
| AS608 | UART2 TX/RX | GPIO17/GPIO16 |
| DHT11 | DATA | GPIO6 |
| CO2 | UART0 TX/RX | GPIO4/GPIO5 |

CO2 当前预留 UART0。如果要启用 CO2，需要先把 ESP-IDF console 从 UART0 切到 USB Serial/JTAG，否则会和日志串口冲突。

---

## 编译与烧录

准备 ESP-IDF 环境后执行：

```bash
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

常用配置项：

- WiFi SSID 和密码。
- MQTT/OneNET 产品 ID、设备名、设备 ID、Broker 地址。
- LVGL 内存配置，当前建议使用 C library malloc。
- 调试开关 `CLEAR_BIOMETRIC_DB_ON_BOOT`，正常演示前要确认是否关闭。

---

## 调试提示

- 如果 UI 和 WiFi 同时运行出现复位，先看 [docs/WIFI_LCD_LVGL_RESET_INVESTIGATION.md](docs/WIFI_LCD_LVGL_RESET_INVESTIGATION.md)，再看任务栈水位和 LVGL 调用路径。
- 如果刷卡没反应，先确认 RC522 I2C 地址、SDA/SCL 接线和 `app_rfid` 启动日志。
- 如果管理员页或记录页文字显示方框，优先检查 `components/ui_custom/events_init.c` 中使用的字体是否包含对应汉字。
- 如果考勤记录没有上云，先看 `/records/attendance_upload_pending.csv` 是否有待上传记录，再看 event reply 是否返回 `code=200`。

---

## 许可证

Apache-2.0
