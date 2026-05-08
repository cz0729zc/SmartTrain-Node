#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_attendance_start(void);
void app_attendance_handle_card_scan(const char *uid_str);

#ifdef __cplusplus
}
#endif
