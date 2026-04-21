#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 测试聚合模块：统一暴露测试/自检 API，避免 main.c 堆积调试代码。
 */

/**
 * @brief 运行启动阶段的可选自检（受 app_test_hub.c 中 RUN_* 宏控制）。
 */
void app_test_hub_run_startup_tests(void);

/**
 * @brief 启动可选演示任务（受 app_test_hub.c 中 RUN_* 宏控制）。
 */
void app_test_hub_start_optional_tasks(void);

/**
 * @brief 可选 RFID 读写调试（仅调试阶段使用，默认不调用）。
 */
void app_test_hub_rfid_read_write_test(void);

#ifdef __cplusplus
}
#endif
