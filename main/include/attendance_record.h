#pragma once

#include <stdint.h>
#include <time.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t attendance_record_init(void);

esp_err_t attendance_record_append_success(time_t ts,
                                           const char *time_text,
                                           const char *student_id,
                                           const char *card_uid,
                                           const char *method,
                                           uint16_t face_user_id,
                                           uint16_t finger_page_id);

#ifdef __cplusplus
}
#endif
