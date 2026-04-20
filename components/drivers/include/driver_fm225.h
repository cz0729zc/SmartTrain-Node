#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRIVER_FM225_SYNC_H 0xEF
#define DRIVER_FM225_SYNC_L 0xAA

#define DRIVER_FM225_DEFAULT_BAUD_RATE 115200U
#define DRIVER_FM225_DEFAULT_TIMEOUT_MS 3000U
#define DRIVER_FM225_MAX_DATA_LEN      256U

typedef enum {
    DRIVER_FM225_MID_REPLY = 0x00,
    DRIVER_FM225_MID_NOTE = 0x01,
    DRIVER_FM225_MID_IMAGE = 0x02,
    DRIVER_FM225_MID_RESET = 0x10,
    DRIVER_FM225_MID_GET_STATUS = 0x11,
    DRIVER_FM225_MID_VERIFY = 0x12,
    DRIVER_FM225_MID_ENROLL = 0x13,
    DRIVER_FM225_MID_ENROLL_SINGLE = 0x1D,
    DRIVER_FM225_MID_DELETE_USER = 0x20,
    DRIVER_FM225_MID_DELETE_ALL = 0x21,
    DRIVER_FM225_MID_GET_USER_INFO = 0x22,
    DRIVER_FM225_MID_FACE_RESET = 0x23,
    DRIVER_FM225_MID_GET_ALL_USER_ID = 0x24,
    DRIVER_FM225_MID_ENROLL_ITG = 0x26,
    DRIVER_FM225_MID_GET_VERSION = 0x30,
} driver_fm225_mid_t;

typedef enum {
    DRIVER_FM225_RESULT_SUCCESS = 0,
    DRIVER_FM225_RESULT_REJECTED = 1,
    DRIVER_FM225_RESULT_ABORTED = 2,
    DRIVER_FM225_RESULT_FAILED_CAMERA = 4,
    DRIVER_FM225_RESULT_FAILED_UNKNOWN = 5,
    DRIVER_FM225_RESULT_FAILED_INVALID_PARAM = 6,
    DRIVER_FM225_RESULT_FAILED_NO_MEMORY = 7,
    DRIVER_FM225_RESULT_FAILED_UNKNOWN_USER = 8,
    DRIVER_FM225_RESULT_FAILED_MAX_USER = 9,
    DRIVER_FM225_RESULT_FAILED_FACE_ENROLLED = 10,
    DRIVER_FM225_RESULT_FAILED_LIVENESS = 12,
    DRIVER_FM225_RESULT_FAILED_TIMEOUT = 13,
    DRIVER_FM225_RESULT_FAILED_AUTH = 14,
    DRIVER_FM225_RESULT_FAILED_READ_FILE = 19,
    DRIVER_FM225_RESULT_FAILED_WRITE_FILE = 20,
    DRIVER_FM225_RESULT_FAILED_NO_ENCRYPT = 21,
    DRIVER_FM225_RESULT_FAILED_NO_RGB_IMAGE = 23,
    DRIVER_FM225_RESULT_FAILED_JPG_LARGE = 24,
    DRIVER_FM225_RESULT_FAILED_JPG_SMALL = 25,
} driver_fm225_result_t;

typedef enum {
    DRIVER_FM225_STATUS_IDLE = 0,
    DRIVER_FM225_STATUS_BUSY = 1,
    DRIVER_FM225_STATUS_ERROR = 2,
    DRIVER_FM225_STATUS_INVALID = 3,
} driver_fm225_status_t;

typedef enum {
    DRIVER_FM225_NID_READY = 0,
    DRIVER_FM225_NID_FACE_STATE = 1,
    DRIVER_FM225_NID_UNKNOWN_ERROR = 2,
    DRIVER_FM225_NID_OTA_DONE = 3,
    DRIVER_FM225_NID_EYE_STATE = 4,
} driver_fm225_nid_t;

typedef enum {
    DRIVER_FM225_FACE_DIR_UNDEFINE = 0x00,
    DRIVER_FM225_FACE_DIR_MIDDLE = 0x01,
    DRIVER_FM225_FACE_DIR_RIGHT = 0x02,
    DRIVER_FM225_FACE_DIR_LEFT = 0x04,
    DRIVER_FM225_FACE_DIR_DOWN = 0x08,
    DRIVER_FM225_FACE_DIR_UP = 0x10,
} driver_fm225_face_dir_t;

typedef enum {
    DRIVER_FM225_ENROLL_INTERACTIVE = 0x00,
    DRIVER_FM225_ENROLL_SINGLE = 0x01,
} driver_fm225_enroll_type_t;

typedef struct {
    uint8_t msg_id;
    uint16_t size;
    uint8_t data[DRIVER_FM225_MAX_DATA_LEN];
} driver_fm225_frame_t;

typedef struct {
    uint8_t mid;
    uint8_t result;
    uint16_t data_len;
    uint8_t data[DRIVER_FM225_MAX_DATA_LEN - 2];
} driver_fm225_reply_t;

typedef struct {
    uint16_t user_id;
    char user_name[33];
    uint8_t admin;
    uint8_t unlock_status;
} driver_fm225_verify_result_t;

typedef struct {
    uint16_t user_id;
    char user_name[33];
    uint8_t admin;
} driver_fm225_user_info_t;

typedef struct {
    uart_port_t uart_num;
    gpio_num_t tx_gpio;
    gpio_num_t rx_gpio;
    uint32_t baud_rate;
    uint32_t cmd_timeout_ms;
    int rx_buffer_size;
} driver_fm225_config_t;

esp_err_t driver_fm225_init(const driver_fm225_config_t *config);
esp_err_t driver_fm225_deinit(void);

esp_err_t driver_fm225_wait_ready(uint32_t timeout_ms);
esp_err_t driver_fm225_reset(uint8_t *result_code);
esp_err_t driver_fm225_get_status(driver_fm225_status_t *status, uint8_t *result_code);

esp_err_t driver_fm225_verify(uint8_t power_down_rightaway, uint8_t timeout_s,
                              driver_fm225_verify_result_t *verify_result,
                              uint8_t *result_code);

esp_err_t driver_fm225_enroll_itg(uint8_t admin, const char *user_name,
                                  driver_fm225_enroll_type_t enroll_type,
                                  uint8_t allow_duplicate, uint8_t timeout_s,
                                  uint16_t *user_id, uint8_t *face_direction_bitmap,
                                  uint8_t *result_code);

esp_err_t driver_fm225_delete_user(uint16_t user_id, uint8_t *result_code);
esp_err_t driver_fm225_delete_all(uint8_t *result_code);
esp_err_t driver_fm225_get_user_info(uint16_t user_id, driver_fm225_user_info_t *user_info,
                                     uint8_t *result_code);

esp_err_t driver_fm225_read_frame(driver_fm225_frame_t *frame, uint32_t timeout_ms);
const char *driver_fm225_result_message(uint8_t result_code);

#ifdef __cplusplus
}
#endif
