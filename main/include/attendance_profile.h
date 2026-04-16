#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* UID 字符串（如 "04:A1:B2:C3:D4"）预留长度 */
#define ATTENDANCE_UID_MAX_LEN 32
/* 学员编号字符串预留长度 */
#define ATTENDANCE_STUDENT_ID_MAX_LEN 24
/* 学员姓名字符串预留长度（UTF-8） */
#define ATTENDANCE_NAME_MAX_LEN 32

/**
 * @brief 学员档案
 *
 * 通过 RFID UID 绑定学号与姓名，供刷卡时快速查询。
 */
typedef struct {
    char uid[ATTENDANCE_UID_MAX_LEN];
    char student_id[ATTENDANCE_STUDENT_ID_MAX_LEN];
    char name[ATTENDANCE_NAME_MAX_LEN];
} attendance_profile_t;

/**
 * @brief 初始化档案库
 *
 * 从 NVS 加载历史档案到内存；首次启动时创建空档案库。
 */
esp_err_t attendance_profile_init(void);

/**
 * @brief 新增或更新档案
 *
 * 若 UID 已存在则更新学号/姓名，否则新增一条并持久化到 NVS。
 */
esp_err_t attendance_profile_upsert(const char *uid, const char *student_id, const char *name);

/**
 * @brief 按 UID 查询档案
 */
esp_err_t attendance_profile_find_by_uid(const char *uid, attendance_profile_t *out_profile);

/**
 * @brief 获取当前档案数量
 */
size_t attendance_profile_count(void);

#ifdef __cplusplus
}
#endif
