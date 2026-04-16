#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver_as608.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_fingerprint_init(void);
esp_err_t app_fingerprint_deinit(void);

/* 两次采图建模并入库，对应 STM32 示例 Add_FR 的核心流程。 */
esp_err_t app_fingerprint_enroll(uint16_t page_id, uint8_t *confirm_code);

/* 采图 + 特征提取 + 高速搜索，对应 STM32 示例 press_FR 的核心流程。 */
esp_err_t app_fingerprint_identify(driver_as608_search_result_t *result, uint8_t *confirm_code);

esp_err_t app_fingerprint_delete(uint16_t page_id, uint8_t *confirm_code);
esp_err_t app_fingerprint_empty(uint8_t *confirm_code);
esp_err_t app_fingerprint_get_valid_count(uint16_t *valid_num, uint8_t *confirm_code);
esp_err_t app_fingerprint_get_sys_para(driver_as608_sys_para_t *sys_para, uint8_t *confirm_code);

const char *app_fingerprint_confirm_message(uint8_t confirm_code);

#ifdef __cplusplus
}
#endif
