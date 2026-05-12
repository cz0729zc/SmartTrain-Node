#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver_fm225.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uart_port_t uart_num;
    gpio_num_t tx_gpio;
    gpio_num_t rx_gpio;
    uint32_t baud_rate;
    uint32_t cmd_timeout_ms;
} app_face_config_t;

esp_err_t app_face_init(const app_face_config_t *config);
esp_err_t app_face_deinit(void);

esp_err_t app_face_wait_ready(uint32_t timeout_ms);
esp_err_t app_face_get_status(driver_fm225_status_t *status, uint8_t *result_code);

esp_err_t app_face_verify_once(uint8_t timeout_s, driver_fm225_verify_result_t *verify_result,
                               uint8_t *result_code);

esp_err_t app_face_enroll_single(const char *user_name, bool admin, uint8_t timeout_s,
                                 uint16_t *user_id, uint8_t *result_code);

esp_err_t app_face_delete_all(uint8_t *result_code);

const char *app_face_result_message(uint8_t result_code);

#ifdef __cplusplus
}
#endif
