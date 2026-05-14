#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ATTENDANCE_RECORD_MAX_TEXT_LEN 32

typedef struct {
    char time_text[ATTENDANCE_RECORD_MAX_TEXT_LEN];
    char student_id[ATTENDANCE_RECORD_MAX_TEXT_LEN];
    char card_uid[ATTENDANCE_RECORD_MAX_TEXT_LEN];
    char method[ATTENDANCE_RECORD_MAX_TEXT_LEN];
    char result[ATTENDANCE_RECORD_MAX_TEXT_LEN];
} attendance_record_item_t;

esp_err_t attendance_record_init(void);

esp_err_t attendance_record_append_success(time_t ts,
                                           const char *time_text,
                                           const char *student_id,
                                           const char *card_uid,
                                           const char *method,
                                           uint16_t face_user_id,
                                           uint16_t finger_page_id);

esp_err_t attendance_record_read_recent(attendance_record_item_t *items,
                                        size_t max_items,
                                        size_t *out_count);

esp_err_t attendance_record_clear(void);

#ifdef __cplusplus
}
#endif
