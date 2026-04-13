#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ATTENDANCE_UID_MAX_LEN 32
#define ATTENDANCE_STUDENT_ID_MAX_LEN 24
#define ATTENDANCE_NAME_MAX_LEN 32

typedef struct {
    char uid[ATTENDANCE_UID_MAX_LEN];
    char student_id[ATTENDANCE_STUDENT_ID_MAX_LEN];
    char name[ATTENDANCE_NAME_MAX_LEN];
} attendance_profile_t;

esp_err_t attendance_profile_init(void);

esp_err_t attendance_profile_upsert(const char *uid, const char *student_id, const char *name);

esp_err_t attendance_profile_find_by_uid(const char *uid, attendance_profile_t *out_profile);

size_t attendance_profile_count(void);

#ifdef __cplusplus
}
#endif
