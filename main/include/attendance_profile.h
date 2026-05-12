#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
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

/* 人脸/指纹 ID 的未绑定哨兵值 */
#define ATTENDANCE_FACE_ID_UNBOUND   0xFFFFU
#define ATTENDANCE_FINGER_ID_UNBOUND 0xFFFFU

/**
 * @brief 学员档案
 *
 * 通过 RFID UID 绑定学号与姓名，供刷卡时快速查询。
 */
typedef struct {
    char uid[ATTENDANCE_UID_MAX_LEN];
    char student_id[ATTENDANCE_STUDENT_ID_MAX_LEN];
    char name[ATTENDANCE_NAME_MAX_LEN];
    uint16_t face_user_id;
    uint16_t finger_page_id;
    bool has_face_bound;
    bool has_finger_bound;
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
 * name 可为空字符串（不再强制要求姓名）。
 */
esp_err_t attendance_profile_upsert(const char *uid, const char *student_id, const char *name);

/**
 * @brief 按 UID 查询档案
 */
esp_err_t attendance_profile_find_by_uid(const char *uid, attendance_profile_t *out_profile);

/**
 * @brief 按学号查询档案
 */
esp_err_t attendance_profile_find_by_student_id(const char *student_id,
                                                attendance_profile_t *out_profile);

/**
 * @brief 按人脸 ID 查询档案
 */
esp_err_t attendance_profile_find_by_face_id(uint16_t face_user_id,
                                             attendance_profile_t *out_profile);

/**
 * @brief 按指纹库页号查询档案
 */
esp_err_t attendance_profile_find_by_finger_page(uint16_t finger_page_id,
                                                 attendance_profile_t *out_profile);

/**
 * @brief 绑定/更新指定 UID 对应的人脸 ID
 */
esp_err_t attendance_profile_bind_face_id(const char *uid, uint16_t face_user_id);

/**
 * @brief 绑定/更新指定 UID 对应的指纹库页号
 */
esp_err_t attendance_profile_bind_finger_page(const char *uid, uint16_t finger_page_id);

/**
 * @brief 获取当前档案数量
 */
size_t attendance_profile_count(void);

/**
 * @brief Print the current in-memory/NVS-backed profile table to the serial log.
 */
void attendance_profile_dump(void);

#ifdef __cplusplus
}
#endif
