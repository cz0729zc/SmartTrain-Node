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

---

## 6. Git 版本控制规范 (Git Version Control)

### 6.1 分支管理策略 (Branching Model)

采用简化的 Git Flow 模型：

*   **main / master**：主分支，永远保持稳定，随时可发布。仅接受来自 `develop` 或 `hotfix` 的合并。
*   **develop**：开发主分支，包含最新功能。
*   **feature/xxx**：功能分支，从 `develop` 检出，完成后合并回 `develop`。
    *   命名：`feature/wifi_connect`, `feature/sensor_driver`
*   **fix/xxx**：修复分支，用于修复 `develop` 上的非紧急 bug。
*   **hotfix/xxx**：紧急修复分支，从 `main` 检出，修复后合并回 `main` 和 `develop`。

### 6.2 提交信息规范 (Commit Messages)

必须遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范。

格式：`<type>(<scope>): <subject>`

*   **type**：
    *   `feat`: 新功能 (feature)
    *   `fix`: 修复 bug
    *   `docs`: 文档变更
    *   `style`: 代码格式调整（不影响逻辑，如空格、分号）
    *   `refactor`: 代码重构（既不修复 bug 也不添加功能）
    *   `perf`: 性能优化
    *   `test`: 添加或修改测试
    *   `chore`: 构建过程或辅助工具变动
*   **scope**（可选）：影响的范围，如 `wifi`, `bsp`, `main`。
*   **subject**：简短描述，使用祈使句（如 "add wifi support" 而非 "added wifi support"）。

**示例**：
> `feat(wifi): add auto-reconnect logic`  
> `fix(sensor): resolve i2c timeout issue`  
> `docs: update readme with pin definition`

### 6.3 忽略文件 (.gitignore)

必须确保 `.gitignore` 包含以下内容，避免提交垃圾文件：
*   **构建产物**：`build/`, `*.elf`, `*.bin`, `*.map`, `*.o`
*   **本地配置**：`sdkconfig`, `sdkconfig.old` (应提交 `sdkconfig.defaults`)
*   **IDE配置**：`.vscode/` (保留 `c_cpp_properties.json` 等协作配置除外), `.idea/`
*   **系统文件**：`.DS_Store`, `Thumbs.db`

### 6.4 代码审查与合并 (Code Review & Merge)

1.  **Pull Request (PR) / Merge Request (MR)**：
    *   禁止直接 Push 到 `main` 或 `develop`。
    *   所有代码变更必须通过 PR 合并。
2.  **代码审查 (Code Review)**：
    *   至少需要 1 名其他开发人员 Review 通过。
    *   检查点：代码风格、潜在 Bug、资源泄漏、是否符合模块化规范。
3.  **合并策略**：
    *   推荐使用 **Squash and Merge**（将功能分支的多次提交压缩为一次），保持主分支历史整洁。

### 6.5 版本发布 (Release)

*   **Tag**：发布新版本时，必须在 `main` 分支打 Tag。
*   **语义化版本 (Semantic Versioning)**：`v<Major>.<Minor>.<Patch>`
    *   `v1.0.0`: 初始正式版
    *   `v1.1.0`: 添加向下兼容的新功能
    *   `v1.1.1`: 向下兼容的 Bug 修复
