#pragma once

#include <stdbool.h>
#include <stdint.h>

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

/**
 * @brief 人脸注册测试接口（单次调用）。
 * @param user_name 用户名（最长 32 字节，NULL 时使用默认名）
 * @param admin 是否管理员
 * @param timeout_s 注册超时秒数（0 时使用默认值）
 */
void app_test_hub_face_enroll_test(const char *user_name, bool admin, uint8_t timeout_s);

/**
 * @brief 人脸识别测试接口（单次调用）。
 * @param timeout_s 识别超时秒数（0 时使用默认值）
 */
void app_test_hub_face_identify_test(uint8_t timeout_s);

#ifdef __cplusplus
}
#endif
