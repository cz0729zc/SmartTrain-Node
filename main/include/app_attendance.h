#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_attendance_start(void);
void app_attendance_handle_card_scan(const char *uid_str);
esp_err_t app_attendance_enter_card_write_mode(void);
esp_err_t app_attendance_exit_card_write_mode(void);
bool app_attendance_is_card_write_mode(void);
esp_err_t app_attendance_register_face_selected(void);
esp_err_t app_attendance_register_fingerprint_selected(void);
esp_err_t app_attendance_exit_admin_mode(void);

#ifdef __cplusplus
}
#endif
