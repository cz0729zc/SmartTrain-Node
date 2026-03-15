# ESP32 嵌入式软件开发规范手册

## 1. 前言

本手册旨在建立一套标准化的 ESP32 嵌入式软件开发流程，确保代码的高质量、可维护性、可扩展性及团队协作的顺畅性。本规范基于 ESP-IDF 框架，融合了模块化、高内聚、低耦合的设计思想。

---

## 2. 工程架构 (Project Architecture)

### 2.1 目录结构标准

工程应遵循 ESP-IDF 组件化架构，严格分离应用逻辑与驱动模块。

```text
project_root/
├── CMakeLists.txt              // 项目顶层构建脚本
├── sdkconfig                   // 项目当前配置（不应提交到版本库，提交 sdkconfig.defaults）
├── components/                 // 【核心】通用功能组件库
│   ├── app_wifi/               // 例如：WiFi 管理组件
│   │   ├── CMakeLists.txt
│   │   ├── include/            // 对外头文件（API）
│   │   │   └── app_wifi.h
│   │   └── app_wifi.c          // 内部实现（Hidden）
│   └── driver_sensor/          // 其他组件...
├── main/                       // 应用层入口
│   ├── CMakeLists.txt
│   ├── main.c                  // app_main 入口，仅负责初始化和调度
│   └── app_controller.c        // 业务逻辑控制
└── docs/                       // 文档
```

### 2.2 模块化设计原则 (Modularity)

*   **高内聚 (High Cohesion)**：每个组件（Component）应只负责单一功能（如 WiFi 仅负责连接维护，不负责发送 MQTT 数据）。
*   **低耦合 (Low Coupling)**：组件之间禁止通过全局变量通信。
    *   **推荐**：使用 Event Loop (事件循环)、Queue (队列) 或 Callback (回调)。
    *   **禁止**：组件 A 直接 include 组件 B 的源文件或访问其静态变量。
*   **私有化实现**：
    *   `.h` 文件放在 `include/` 目录，仅暴露必要的 API 接口。
    *   `.c` 文件及其辅助函数、结构体定义应在源文件中应用 `static` 修饰，对外部不可见。

---

## 3. 编码规范 (Coding Standard)

### 3.1 命名约定

*   **文件命名**：使用小写字母加下划线，如 `app_wifi.c`。
*   **函数命名**：
    *   **全局 API**：`[组件名]_[动词]_[名词]`，例如 `app_wifi_init()`, `app_wifi_start()`。
    *   **静态函数**：`static` 修饰，动词开头即可，或者加 `_` 前缀，如 `static void connect_handler()`。
*   **变量命名**：
    *   **全局变量**（应尽量避免）：`g_` 前缀，如 `g_system_state`。
    *   **静态/模块级变量**：`s_` 前缀，如 `s_wifi_event_group`。
    *   **常量/宏**：全大写，`APP_WIFI_MAX_RETRY`。

### 3.2 错误处理

*   所有可能失败的函数不仅要打印日志，必须返回 `esp_err_t`。
*   调用 ESP-IDF API 时，必须使用 `ESP_ERROR_CHECK()` (仅限初始化阶段) 或手动检查返回值。

```c
// 推荐
esp_err_t ret = esp_wifi_init(&cfg);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
    return ret;
}
```

### 3.3 日志管理

*   每个 `.c` 文件必须定义独立的 TAG：
    ```c
    static const char *TAG = "app_wifi";
    ```
*   使用正确的日志级别：
    *   `ESP_LOGE`: 错误，系统无法继续某项功能。
    *   `ESP_LOGW`: 警告，异常但可恢复。
    *   `ESP_LOGI`: 关键流程信息（启动、连接成功）。
    *   `ESP_LOGD`: 调试信息（数据包内容），发布时通常关闭。

---

## 4. 开发流程 (Workflow)

1.  **需求分析**：明确功能模块。
2.  **组件划分**：检查 `components` 目录下是否已有类似组件，决定是新建还是复用。
3.  **接口定义**：先编写 `.h` 文件，定义 `init`, `deinit`, `process` 等接口。
4.  **实现与测试**：编写 `.c` 实现，并编写单元测试验证。
5.  **集成**：在 `main.c` 中调用。

---

## 5. FreeRTOS 使用规范

*   **任务 (Task)**：
    *   避免在 `app_main` 中写死循环，`app_main` 也是一个任务，用完即返回或删除。
    *   任务栈大小应通过 `uxTaskGetStackHighWaterMark` 调试确定，避免浪费。
*   **同步**：
    *   优先使用 `EventGroup` 处理状态标志。
    *   优先使用 `Queue` 处理数据流。
    *   慎用 `Semaphore` 互斥锁，尽量通过架构设计避免资源竞争。
