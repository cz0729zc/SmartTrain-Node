# 基于物联网的企业培训管理平台 - 智能终端固件 (Smart Training Terminal)

## 1. 项目简介 (Project Overview)

本项目是“基于物联网的企业培训管理平台”的边端智能终端固件。系统围绕“签到—学习—考核—评价”全流程，构建了一套完整的数据流转模型。该终端主要负责现场的人员身份核验、环境感知以及数据上报，旨在解决传统培训场景中考勤难、环境不可控等问题。

硬件核心采用 **ESP32-S3**，通过模块化设计集成多种传感器与外设，利用并发任务架构处理采集、核验、通信、存储与交互。

---

## 2. 核心功能 (Key Features)

### 2.1 业务流程
- **多模态身份核验**：支持人脸识别（UART模组）、指纹识别（UART/SPI）、刷卡（I2C/SPI）。
- **全流程管理**：覆盖到场签到、学习过程监测、考核记录及评价反馈。
- **状态机控制**：严格的状态流转设计，确保业务逻辑的一致性。

### 2.2 数据可靠性 & 安全性
- **断网续传**：支持本地 SD 卡缓存数据，网络恢复后自动重传，保证数据零丢失。
- **掉电保护**：关键数据实时写入非易失性存储。
- **安全通信**：MQTT 启用 TLS 加密与 QoS 1 机制，确保数据传输安全可靠。
- **隐私保护**：边端仅存储加密后的特征模板，云端仅记录匿名 ID，符合隐私合规要求。

### 2.3 环境感知
- **实时监测**：集成 CO2 传感器与温湿度传感器，实时监控培训环境舒适度。
- **异常告警**：本地声光报警（蜂鸣器/屏幕）及云端推送（CO2 超限、缺勤等）。

### 2.4 设备管理
- **配网方式**：支持 SoftAP 热点配网或串口命令行配网。
- **远程运维**：支持 OTA 固件升级、远程配置下发、自检日志上报。
- **设备遗嘱 (LWT)**：利用 MQTT 遗嘱机制管理设备上下线状态。

---

## 3. 硬件架构 (Hardware Architecture)

- **主控芯片**：Espressif ESP32-S3 (双核 Xtensa® LX7, AI 加速)
- **外设接口**：
    - **UART**: 摄像头模组 / 人脸识别模组
    - **UART/SPI**: 指纹传感器
    - **I2C/SPI**: 刷卡模块 (NFC/RFID)
    - **I2C**: CO2 传感器, 温湿度传感器, OLED/LCD 显示屏
    - **GPIO**: 蜂鸣器, 按键, LED 指示灯
    - **SDIO/SPI**: SD 卡 (数据存储)
- **电源管理**：抗干扰设计，静电防护，RTC 独立供电维持离线时钟。

---

## 4. 软件架构 (Software Architecture)

本项目基于 **ESP-IDF** 框架开发，采用模块化、分层设计。

### 4.1 目录结构
```text
Smart_Training_Terminal/
├── components/                 // 功能组件
│   ├── app_wifi/               // WiFi 连接与配网
│   ├── bsp/                    // 板级支持包 (GPIO, I2C, SPI 初始化)
│   ├── common/                 // 通用工具 (队列, 数据结构, 宏)
│   ├── drivers/                // 外设驱动 (传感器, 屏幕, 指纹)
│   ├── middleware/             // 中间件 (MQTT, 数据存储, 协议解析) - 规划中
│   └── ui/                     // 用户交互逻辑 - 规划中
├── main/                       // 业务主逻辑
│   ├── main.c                  // 系统入口与任务调度
│   └── app_controller.c        // 核心状态机
├── docs/                       // 文档
└── tools/                      // 辅助脚本
```

### 4.2 关键技术点
- **并发任务架构**：独立任务处理采集、核验、网络通信、本地存储与 UI 交互。
- **消息队列**：任务间通过 FreeRTOS Queue 进行低耦合通信。
- **MQTT 主题规划**：
    - `telemetry`: 传感器数据
    - `attendance`: 考勤记录 (UUID + 时间戳)
    - `events`: 告警事件
    - `config`: 远程配置下发

---

## 5. 快速开始 (Getting Started)

### 5.1 环境准备
- 安装 [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html) 开发环境 (VS Code 插件推荐)。
- 目标芯片设置为 `esp32s3`。

### 5.2 编译与烧录
1. **配置项目**：
   ```bash
   idf.py menuconfig
   # 配置 WiFi SSID/Password (或使用配网功能)
   # 配置 MQTT Broker 地址
   ```

2. **编译**：
   ```bash
   idf.py build
   ```

3. **烧录**：
   ```bash
   idf.py -p PORT flash monitor
   ```

---

## 6. 开发规范
请参考 [ESP32 嵌入式软件开发规范手册](./ESP32_Development_Standards.md)。
主要遵循：
- **模块化**：功能封装在 `components/` 下。
- **Git 规范**：使用 Conventional Commits 提交代码。
- **代码风格**：遵循 ESP-IDF 编程指南。

---

## 7. 许可证 (License)
Apache-2.0
