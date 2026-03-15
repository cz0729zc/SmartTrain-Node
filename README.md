# EntTrain-Edge: 基于物联网的企业培训管理平台 (边缘端)| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 |

| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)

[![Framework](https://img.shields.io/badge/Framework-ESP--IDF-green.svg)](https://docs.espressif.com/projects/esp-idf/)# Wi-Fi Station Example



## 1. 项目概述 (Overview)(See the README.md file in the upper level 'examples' directory for more information about examples.)



本项目是 **企业培训管理平台** 的边缘硬件端实现，针对“签到—学习—考核—评价”全流程设计。系统负责现场的数据采集、身份核验、环境感知与规则响应，并通过 MQTT 协议与云端平台进行安全通信。This example shows how to use the Wi-Fi Station functionality of the Wi-Fi driver of ESP for connecting to an Access Point.



核心目标是解决断网、掉电、多人并发等异常场景下的数据一致性问题，确保所有操作可追溯、可审计。## How to use example



## 2. 硬件架构 (Hardware Architecture)### Configure the project



*   **核心控制器**: [ESP32-S3](https://www.espressif.com/en/products/socs/esp32-s3) (Xtensa® 32-bit LX7 dual-core)Open the project configuration menu (`idf.py menuconfig`).

*   **身份核验**:

    *   **人脸模组**: UART 接口集成In the `Example Configuration` menu:

    *   **指纹/刷卡**: I2C/SPI 接口连接

*   **环境感知**:* Set the Wi-Fi configuration.

    *   **CO2 传感器**: 监测空气质量 (I2C/UART)    * Set `WiFi SSID`.

    *   **温湿度传感器**: 监测环境舒适度 (I2C)    * Set `WiFi Password`.

*   **存储与交互**:

    *   **SD Card**: 本地离线存储、断点续传缓存Optional: If you need, change the other options according to your requirements.

    *   **RTC**: 维持离线时钟

    *   **HMI**: 显示屏 (SPI/8080) + 蜂鸣器 (PWM)### Build and Flash



## 3. 软件架构 (Software Architecture)Build the project and flash it to the board, then run the monitor tool to view the serial output:



基于 **ESP-IDF** 框架开发，采用模块化与高内聚设计。Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.



### 3.1 核心功能模块(To exit the serial monitor, type ``Ctrl-]``.)

1.  **数据流转模型**: 围绕“到场事件、身份核验、环境感知、规则响应”设计状态机。

2.  **并发任务架构**: 分离采集、核验、通信、存储与 UI 任务。See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

3.  **消息队列**: 任务间通过 FreeRTOS Queue/EventGroup 解耦。

* [ESP-IDF Getting Started Guide on ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

### 3.2 通信与安全* [ESP-IDF Getting Started Guide on ESP32-S2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)

*   **协议**: MQTT v3.1.1 / v5.0* [ESP-IDF Getting Started Guide on ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html)

*   **安全**: TLS 1.2/1.3 加密传输

*   **QoS**: Level 1 (至少一次交付)，配合应用层 ACK## Example Output

*   **设备状态**: 利用 MQTT Last Will (遗嘱) 监控设备在线状态Note that the output, in particular the order of the output, may vary depending on the environment.



### 3.3 数据接口定义 (MQTT Topics)Console output if station connects to AP successfully:

| Topic 类型 | 描述 | 数据内容 |```

| :--- | :--- | :--- |I (589) wifi station: ESP_WIFI_MODE_STA

| `telemetry` | 环境遥测 | 温湿度、CO2 数值、设备运行状态 |I (599) wifi: wifi driver task: 3ffc08b4, prio:23, stack:3584, core=0

| `attendance` | 考勤上报 | 用户 ID、识别方式、时间戳、置信度 |I (599) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE

| `events` | 告警事件 | CO2 超限、非法闯入、设备故障 |I (599) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE

| `config` | 远程配置 | 下发阈值、更新特征库、控制指令 |I (629) wifi: wifi firmware version: 2d94f02

I (629) wifi: config NVS flash: enabled

## 4. 目录结构 (Directory Structure)I (629) wifi: config nano formatting: disabled

I (629) wifi: Init dynamic tx buffer num: 32

```textI (629) wifi: Init data frame dynamic rx buffer num: 32

EntTrain-Edge/I (639) wifi: Init management frame dynamic rx buffer num: 32

├── components/I (639) wifi: Init management short buffer num: 32

│   ├── app_wifi/        # WiFi 连接与重连管理I (649) wifi: Init static rx buffer size: 1600

│   ├── bsp/             # 板级支持包 (GPIO, I2C, SPI 初始化)I (649) wifi: Init static rx buffer num: 10

│   ├── common/          # 通用工具 (JSON 解析, 数据结构)I (659) wifi: Init dynamic rx buffer num: 32

│   └── drivers/         # 传感器与外设驱动 (CO2, Screen, Camera)I (759) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 0

├── main/I (769) wifi: mode : sta (30:ae:a4:d9:bc:c4)

│   ├── app_controller.c # 业务逻辑主控I (769) wifi station: wifi_init_sta finished.

│   └── main.c           # 系统启动入口I (889) wifi: new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1

├── docs/                # 设计文档与手册I (889) wifi: state: init -> auth (b0)

└── tools/               # 调试脚本I (899) wifi: state: auth -> assoc (0)

```I (909) wifi: state: assoc -> run (10)

I (939) wifi: connected with #!/bin/test, aid = 1, channel 6, BW20, bssid = ac:9e:17:7e:31:40

## 5. 开发环境搭建 (Setup)I (939) wifi: security type: 3, phy: bgn, rssi: -68

I (949) wifi: pm start, type: 1

1.  **安装 ESP-IDF**: 请参考 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_cn/latest/esp32s3/get-started/index.html)。

2.  **克隆项目**:I (1029) wifi: AP's beacon interval = 102400 us, DTIM period = 3

    ```bashI (2089) esp_netif_handlers: sta ip: 192.168.77.89, mask: 255.255.255.0, gw: 192.168.77.1

    git clone <repository_url>I (2089) wifi station: got ip:192.168.77.89

    cd EntTrain-EdgeI (2089) wifi station: connected to ap SSID:myssid password:mypassword

    ``````

3.  **配置工程**:

    ```bashConsole output if the station failed to connect to AP:

    idf.py set-target esp32s3```

    idf.py menuconfigI (589) wifi station: ESP_WIFI_MODE_STA

    ```I (599) wifi: wifi driver task: 3ffc08b4, prio:23, stack:3584, core=0

    *在 menuconfig 中配置 WiFi SSID、MQTT Broker 地址及设备证书。*I (599) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE

I (599) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE

4.  **编译与烧录**:I (629) wifi: wifi firmware version: 2d94f02

    ```bashI (629) wifi: config NVS flash: enabled

    idf.py buildI (629) wifi: config nano formatting: disabled

    idf.py -p COMx flash monitorI (629) wifi: Init dynamic tx buffer num: 32

    ```I (629) wifi: Init data frame dynamic rx buffer num: 32

I (639) wifi: Init management frame dynamic rx buffer num: 32

## 6. 特性列表 (Features)I (639) wifi: Init management short buffer num: 32

I (649) wifi: Init static rx buffer size: 1600

- [ ] **多模态身份识别**: 支持人脸、指纹、刷卡多种方式。I (649) wifi: Init static rx buffer num: 10

- [ ] **断点续传**: 网络中断时数据写入 SD 卡，恢复后自动上传。I (659) wifi: Init dynamic rx buffer num: 32

- [ ] **安全加密**: 本地特征模板加密存储，云端通信全链路加密。I (759) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 0

- [ ] **OTA 升级**: 支持固件远程空中升级。I (759) wifi: mode : sta (30:ae:a4:d9:bc:c4)

- [ ] **自检日志**: 本地日志按日滚动存储。I (769) wifi station: wifi_init_sta finished.

I (889) wifi: new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1

---I (889) wifi: state: init -> auth (b0)

*Created for Graduation Project: IoT-based Enterprise Training Management Platform.*I (1889) wifi: state: auth -> init (200)

I (1889) wifi: new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
I (1889) wifi station: retry to connect to the AP
I (1899) wifi station: connect to the AP fail
I (3949) wifi station: retry to connect to the AP
I (3949) wifi station: connect to the AP fail
I (4069) wifi: new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
I (4069) wifi: state: init -> auth (b0)
I (5069) wifi: state: auth -> init (200)
I (5069) wifi: new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
I (5069) wifi station: retry to connect to the AP
I (5069) wifi station: connect to the AP fail
I (7129) wifi station: retry to connect to the AP
I (7129) wifi station: connect to the AP fail
I (7249) wifi: new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
I (7249) wifi: state: init -> auth (b0)
I (8249) wifi: state: auth -> init (200)
I (8249) wifi: new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
I (8249) wifi station: retry to connect to the AP
I (8249) wifi station: connect to the AP fail
I (10299) wifi station: connect to the AP fail
I (10299) wifi station: Failed to connect to SSID:myssid, password:mypassword
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
