#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRIVER_AS608_DEFAULT_ADDR 0xFFFFFFFFU

typedef enum {
    DRIVER_AS608_CHAR_BUFFER1 = 0x01,
    DRIVER_AS608_CHAR_BUFFER2 = 0x02,
} driver_as608_char_buffer_t;

typedef struct {
    uint16_t page_id;
    uint16_t match_score;
} driver_as608_search_result_t;

typedef struct {
    uint16_t max_templates;
    uint8_t security_level;
    uint32_t device_addr;
    uint8_t packet_size_code;
    uint8_t baud_factor_n;
} driver_as608_sys_para_t;

typedef struct {
    uart_port_t uart_num;
    gpio_num_t tx_gpio;
    gpio_num_t rx_gpio;
    uint32_t baud_rate;
    uint32_t addr;
    uint32_t cmd_timeout_ms;
} driver_as608_config_t;

/* 模块确认码（手册 2.4.2） */
typedef enum {
    DRIVER_AS608_CONFIRM_OK = 0x00,
    DRIVER_AS608_CONFIRM_PACKET_ERROR = 0x01,
    DRIVER_AS608_CONFIRM_NO_FINGER = 0x02,
    DRIVER_AS608_CONFIRM_IMAGE_FAIL = 0x03,
    DRIVER_AS608_CONFIRM_IMAGE_TOO_DRY = 0x04,
    DRIVER_AS608_CONFIRM_IMAGE_TOO_WET = 0x05,
    DRIVER_AS608_CONFIRM_IMAGE_MESSY = 0x06,
    DRIVER_AS608_CONFIRM_FEW_FEATURE = 0x07,
    DRIVER_AS608_CONFIRM_NOT_MATCH = 0x08,
    DRIVER_AS608_CONFIRM_NOT_FOUND = 0x09,
    DRIVER_AS608_CONFIRM_MERGE_FAIL = 0x0A,
    DRIVER_AS608_CONFIRM_ADDR_OUT_OF_RANGE = 0x0B,
    DRIVER_AS608_CONFIRM_DELETE_FAIL = 0x10,
    DRIVER_AS608_CONFIRM_EMPTY_FAIL = 0x11,
    DRIVER_AS608_CONFIRM_BAD_PASSWORD = 0x13,
    DRIVER_AS608_CONFIRM_INVALID_IMAGE = 0x15,
    DRIVER_AS608_CONFIRM_FLASH_ERROR = 0x18,
    DRIVER_AS608_CONFIRM_UNDEFINED = 0x19,
    DRIVER_AS608_CONFIRM_INVALID_REG = 0x1A,
    DRIVER_AS608_CONFIRM_INVALID_REG_VALUE = 0x1B,
    DRIVER_AS608_CONFIRM_INVALID_NOTEPAD_PAGE = 0x1C,
    DRIVER_AS608_CONFIRM_PORT_FAIL = 0x1D,
    DRIVER_AS608_CONFIRM_AUTO_ENROLL_FAIL = 0x1E,
    DRIVER_AS608_CONFIRM_DB_FULL = 0x1F,
} driver_as608_confirm_code_t;

esp_err_t driver_as608_init(const driver_as608_config_t *config);
esp_err_t driver_as608_deinit(void);

esp_err_t driver_as608_get_image(uint8_t *confirm_code);
esp_err_t driver_as608_gen_char(driver_as608_char_buffer_t buffer_id, uint8_t *confirm_code);
esp_err_t driver_as608_match(uint8_t *confirm_code);
esp_err_t driver_as608_search(driver_as608_char_buffer_t buffer_id, uint16_t start_page,
                              uint16_t page_num, driver_as608_search_result_t *result,
                              uint8_t *confirm_code);
esp_err_t driver_as608_reg_model(uint8_t *confirm_code);
esp_err_t driver_as608_store_char(driver_as608_char_buffer_t buffer_id, uint16_t page_id,
                                  uint8_t *confirm_code);
esp_err_t driver_as608_delete_char(uint16_t page_id, uint16_t count, uint8_t *confirm_code);
esp_err_t driver_as608_empty(uint8_t *confirm_code);
esp_err_t driver_as608_read_sys_para(driver_as608_sys_para_t *sys_para, uint8_t *confirm_code);
esp_err_t driver_as608_high_speed_search(driver_as608_char_buffer_t buffer_id, uint16_t start_page,
                                         uint16_t page_num, driver_as608_search_result_t *result,
                                         uint8_t *confirm_code);
esp_err_t driver_as608_valid_template_num(uint16_t *valid_num, uint8_t *confirm_code);
esp_err_t driver_as608_write_notepad(uint8_t page_num, const uint8_t *note_32_bytes,
                                     uint8_t *confirm_code);
esp_err_t driver_as608_read_notepad(uint8_t page_num, uint8_t *note_32_bytes,
                                    uint8_t *confirm_code);

const char *driver_as608_confirm_message(uint8_t confirm_code);

#ifdef __cplusplus
}
#endif
