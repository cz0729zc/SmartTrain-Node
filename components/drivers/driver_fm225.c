#include "driver_fm225.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_log_buffer.h"
#include "esp_timer.h"

static const char *TAG = "driver_fm225";

#define DRIVER_FM225_ENABLE_HEX_LOG 1

static bool s_inited = false;
static uart_port_t s_uart_num = UART_NUM_MAX;
static uint32_t s_cmd_timeout_ms = DRIVER_FM225_DEFAULT_TIMEOUT_MS;

static uint8_t calc_xor(uint8_t msg_id, uint16_t size, const uint8_t *data)
{
    uint8_t x = msg_id ^ (uint8_t)(size >> 8) ^ (uint8_t)size;
    for (uint16_t i = 0; i < size; ++i) {
        x ^= data[i];
    }
    return x;
}

static esp_err_t read_exact_until(uint8_t *buf, size_t len, int64_t deadline_us)
{
    size_t got = 0;
    while (got < len) {
        int64_t now = esp_timer_get_time();
        if (now >= deadline_us) {
            return ESP_ERR_TIMEOUT;
        }

        uint32_t remain_ms = (uint32_t)((deadline_us - now + 999) / 1000);
        if (remain_ms > 50) {
            remain_ms = 50;
        }

        int n = uart_read_bytes(s_uart_num, buf + got, len - got, pdMS_TO_TICKS(remain_ms));
        if (n < 0) {
            return ESP_FAIL;
        }
        if (n == 0) {
            continue;
        }
        got += (size_t)n;
    }

    return ESP_OK;
}

static esp_err_t recv_frame_until(driver_fm225_frame_t *frame, int64_t deadline_us)
{
    uint8_t prev = 0;
    uint8_t cur = 0;
    esp_err_t ret;

    while (true) {
        ret = read_exact_until(&cur, 1, deadline_us);
        if (ret != ESP_OK) {
            return ret;
        }
        if (prev == DRIVER_FM225_SYNC_H && cur == DRIVER_FM225_SYNC_L) {
            break;
        }
        prev = cur;
    }

    uint8_t head[3] = {0};
    ret = read_exact_until(head, sizeof(head), deadline_us);
    if (ret != ESP_OK) {
        return ret;
    }

    frame->msg_id = head[0];
    frame->size = ((uint16_t)head[1] << 8) | head[2];
    if (frame->size > DRIVER_FM225_MAX_DATA_LEN) {
        ESP_LOGW(TAG, "收到过长帧: msg=0x%02X size=%u", frame->msg_id, (unsigned)frame->size);
        return ESP_ERR_INVALID_SIZE;
    }

    if (frame->size > 0) {
        ret = read_exact_until(frame->data, frame->size, deadline_us);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    uint8_t rx_parity = 0;
    ret = read_exact_until(&rx_parity, 1, deadline_us);
    if (ret != ESP_OK) {
        return ret;
    }

#if DRIVER_FM225_ENABLE_HEX_LOG
    uint8_t raw[2 + 3 + DRIVER_FM225_MAX_DATA_LEN + 1] = {0};
    size_t raw_len = 0;
    raw[raw_len++] = DRIVER_FM225_SYNC_H;
    raw[raw_len++] = DRIVER_FM225_SYNC_L;
    raw[raw_len++] = frame->msg_id;
    raw[raw_len++] = (uint8_t)(frame->size >> 8);
    raw[raw_len++] = (uint8_t)frame->size;
    if (frame->size > 0) {
        memcpy(&raw[raw_len], frame->data, frame->size);
        raw_len += frame->size;
    }
    raw[raw_len++] = rx_parity;
    ESP_LOGI(TAG, "RX frame: msg=0x%02X size=%u", frame->msg_id, (unsigned)frame->size);
    ESP_LOG_BUFFER_HEXDUMP(TAG, raw, raw_len, ESP_LOG_INFO);
#endif

    uint8_t calc = calc_xor(frame->msg_id, frame->size, frame->data);
    if (rx_parity != calc) {
        ESP_LOGW(TAG, "校验失败: rx=0x%02X calc=0x%02X msg=0x%02X", rx_parity, calc, frame->msg_id);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

static esp_err_t send_frame(uint8_t msg_id, const uint8_t *data, uint16_t size)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (size > DRIVER_FM225_MAX_DATA_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t frame[2 + 1 + 2 + DRIVER_FM225_MAX_DATA_LEN + 1] = {0};
    size_t offset = 0;

    frame[offset++] = DRIVER_FM225_SYNC_H;
    frame[offset++] = DRIVER_FM225_SYNC_L;
    frame[offset++] = msg_id;
    frame[offset++] = (uint8_t)(size >> 8);
    frame[offset++] = (uint8_t)size;
    if (size > 0 && data != NULL) {
        memcpy(&frame[offset], data, size);
        offset += size;
    }

    frame[offset++] = calc_xor(msg_id, size, data);

#if DRIVER_FM225_ENABLE_HEX_LOG
    ESP_LOGI(TAG, "TX frame: msg=0x%02X size=%u", msg_id, (unsigned)size);
    ESP_LOG_BUFFER_HEXDUMP(TAG, frame, offset, ESP_LOG_INFO);
#endif

    int w = uart_write_bytes(s_uart_num, frame, offset);
    if (w != (int)offset) {
        return ESP_FAIL;
    }

    return uart_wait_tx_done(s_uart_num, pdMS_TO_TICKS(100));
}

static esp_err_t parse_reply(const driver_fm225_frame_t *frame, driver_fm225_reply_t *reply)
{
    if (frame->msg_id != DRIVER_FM225_MID_REPLY) {
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->size < 2) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    reply->mid = frame->data[0];
    reply->result = frame->data[1];
    reply->data_len = (uint16_t)(frame->size - 2);
    if (reply->data_len > 0) {
        memcpy(reply->data, &frame->data[2], reply->data_len);
    }
    return ESP_OK;
}

static esp_err_t send_cmd_wait_reply(uint8_t cmd, const uint8_t *payload, uint16_t payload_len,
                                     driver_fm225_reply_t *reply, uint32_t timeout_ms)
{
    esp_err_t ret = send_frame(cmd, payload, payload_len);
    if (ret != ESP_OK) {
        return ret;
    }

    int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < deadline_us) {
        driver_fm225_frame_t frame = {0};
        ret = recv_frame_until(&frame, deadline_us);
        if (ret != ESP_OK) {
            return ret;
        }

        if (frame.msg_id == DRIVER_FM225_MID_NOTE) {
            ESP_LOGD(TAG, "收到 NOTE: nid=%u", frame.size > 0 ? (unsigned)frame.data[0] : 0U);
            continue;
        }

        if (frame.msg_id != DRIVER_FM225_MID_REPLY) {
            ESP_LOGD(TAG, "忽略非 REPLY 帧: msg_id=0x%02X", frame.msg_id);
            continue;
        }

        ret = parse_reply(&frame, reply);
        if (ret != ESP_OK) {
            return ret;
        }

        if (reply->mid != cmd) {
            ESP_LOGW(TAG, "收到 REPLY 但 mid 不匹配: expect=0x%02X got=0x%02X", cmd, reply->mid);
            continue;
        }
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

static void set_result_code(const driver_fm225_reply_t *reply, uint8_t *result_code)
{
    if (result_code != NULL) {
        *result_code = reply->result;
    }
}

esp_err_t driver_fm225_init(const driver_fm225_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    uart_config_t uart_cfg = {
        .baud_rate = (int)(config->baud_rate == 0 ? DRIVER_FM225_DEFAULT_BAUD_RATE : config->baud_rate),
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(config->uart_num, &uart_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = uart_set_pin(config->uart_num, config->tx_gpio, config->rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        return ret;
    }

    int rx_buf = config->rx_buffer_size <= 0 ? 1024 : config->rx_buffer_size;
    ret = uart_driver_install(config->uart_num, rx_buf, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    s_uart_num = config->uart_num;
    s_cmd_timeout_ms = (config->cmd_timeout_ms == 0) ? DRIVER_FM225_DEFAULT_TIMEOUT_MS
                                                      : config->cmd_timeout_ms;
    s_inited = true;

    ESP_LOGI(TAG, "FM225 初始化成功: uart=%d tx=%d rx=%d baud=%d", (int)config->uart_num,
             (int)config->tx_gpio, (int)config->rx_gpio, uart_cfg.baud_rate);
    return ESP_OK;
}

esp_err_t driver_fm225_deinit(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = uart_driver_delete(s_uart_num);
    if (ret != ESP_OK) {
        return ret;
    }

    s_inited = false;
    s_uart_num = UART_NUM_MAX;
    return ESP_OK;
}

esp_err_t driver_fm225_wait_ready(uint32_t timeout_ms)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < deadline_us) {
        driver_fm225_frame_t frame = {0};
        esp_err_t ret = recv_frame_until(&frame, deadline_us);
        if (ret != ESP_OK) {
            return ret;
        }

        if (frame.msg_id == DRIVER_FM225_MID_NOTE && frame.size >= 1 &&
            frame.data[0] == DRIVER_FM225_NID_READY) {
            return ESP_OK;
        }
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t driver_fm225_reset(uint8_t *result_code)
{
    driver_fm225_reply_t reply = {0};
    esp_err_t ret = send_cmd_wait_reply(DRIVER_FM225_MID_RESET, NULL, 0, &reply, s_cmd_timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    set_result_code(&reply, result_code);
    return ESP_OK;
}

esp_err_t driver_fm225_get_status(driver_fm225_status_t *status, uint8_t *result_code)
{
    driver_fm225_reply_t reply = {0};
    esp_err_t ret = send_cmd_wait_reply(DRIVER_FM225_MID_GET_STATUS, NULL, 0, &reply,
                                        s_cmd_timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    set_result_code(&reply, result_code);
    if (reply.result == DRIVER_FM225_RESULT_SUCCESS && status != NULL) {
        if (reply.data_len < 1) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        *status = (driver_fm225_status_t)reply.data[0];
    }

    return ESP_OK;
}

esp_err_t driver_fm225_verify(uint8_t power_down_rightaway, uint8_t timeout_s,
                              driver_fm225_verify_result_t *verify_result,
                              uint8_t *result_code)
{
    uint8_t req[2] = {power_down_rightaway ? 1 : 0, timeout_s};
    driver_fm225_reply_t reply = {0};
    esp_err_t ret = send_cmd_wait_reply(DRIVER_FM225_MID_VERIFY, req, sizeof(req), &reply,
                                        s_cmd_timeout_ms + (uint32_t)timeout_s * 1000U);
    if (ret != ESP_OK) {
        return ret;
    }

    set_result_code(&reply, result_code);
    if (reply.result != DRIVER_FM225_RESULT_SUCCESS || verify_result == NULL) {
        return ESP_OK;
    }

    if (reply.data_len < 36) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    verify_result->user_id = ((uint16_t)reply.data[0] << 8) | reply.data[1];
    memcpy(verify_result->user_name, &reply.data[2], 32);
    verify_result->user_name[32] = '\0';
    verify_result->admin = reply.data[34];
    verify_result->unlock_status = reply.data[35];

    return ESP_OK;
}

esp_err_t driver_fm225_enroll_itg(uint8_t admin, const char *user_name,
                                  driver_fm225_enroll_type_t enroll_type,
                                  uint8_t allow_duplicate, uint8_t timeout_s,
                                  uint16_t *user_id, uint8_t *face_direction_bitmap,
                                  uint8_t *result_code)
{
    uint8_t req[40] = {0};
    req[0] = admin ? 1 : 0;

    if (user_name != NULL) {
        size_t n = strnlen(user_name, 32);
        memcpy(&req[1], user_name, n);
    }

    req[33] = DRIVER_FM225_FACE_DIR_UNDEFINE;
    req[34] = (uint8_t)enroll_type;
    req[35] = allow_duplicate;
    req[36] = timeout_s;
    req[37] = 0;
    req[38] = 0;
    req[39] = 0;

    driver_fm225_reply_t reply = {0};
    esp_err_t ret = send_cmd_wait_reply(DRIVER_FM225_MID_ENROLL_ITG, req, sizeof(req), &reply,
                                        s_cmd_timeout_ms + (uint32_t)timeout_s * 1000U);
    if (ret != ESP_OK) {
        return ret;
    }

    set_result_code(&reply, result_code);
    if (reply.result == DRIVER_FM225_RESULT_SUCCESS) {
        if (reply.data_len < 3) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        if (user_id != NULL) {
            *user_id = ((uint16_t)reply.data[0] << 8) | reply.data[1];
        }
        if (face_direction_bitmap != NULL) {
            *face_direction_bitmap = reply.data[2];
        }
    }

    return ESP_OK;
}

esp_err_t driver_fm225_delete_user(uint16_t user_id, uint8_t *result_code)
{
    uint8_t req[2] = {(uint8_t)(user_id >> 8), (uint8_t)user_id};
    driver_fm225_reply_t reply = {0};
    esp_err_t ret = send_cmd_wait_reply(DRIVER_FM225_MID_DELETE_USER, req, sizeof(req), &reply,
                                        s_cmd_timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    set_result_code(&reply, result_code);
    return ESP_OK;
}

esp_err_t driver_fm225_delete_all(uint8_t *result_code)
{
    driver_fm225_reply_t reply = {0};
    esp_err_t ret = send_cmd_wait_reply(DRIVER_FM225_MID_DELETE_ALL, NULL, 0, &reply,
                                        s_cmd_timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    set_result_code(&reply, result_code);
    return ESP_OK;
}

esp_err_t driver_fm225_get_user_info(uint16_t user_id, driver_fm225_user_info_t *user_info,
                                     uint8_t *result_code)
{
    uint8_t req[2] = {(uint8_t)(user_id >> 8), (uint8_t)user_id};
    driver_fm225_reply_t reply = {0};
    esp_err_t ret = send_cmd_wait_reply(DRIVER_FM225_MID_GET_USER_INFO, req, sizeof(req), &reply,
                                        s_cmd_timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    set_result_code(&reply, result_code);
    if (reply.result == DRIVER_FM225_RESULT_SUCCESS && user_info != NULL) {
        if (reply.data_len < 35) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        user_info->user_id = ((uint16_t)reply.data[0] << 8) | reply.data[1];
        memcpy(user_info->user_name, &reply.data[2], 32);
        user_info->user_name[32] = '\0';
        user_info->admin = reply.data[34];
    }

    return ESP_OK;
}

esp_err_t driver_fm225_read_frame(driver_fm225_frame_t *frame, uint32_t timeout_ms)
{
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    return recv_frame_until(frame, deadline_us);
}

const char *driver_fm225_result_message(uint8_t result_code)
{
    switch (result_code) {
        case DRIVER_FM225_RESULT_SUCCESS: return "success";
        case DRIVER_FM225_RESULT_REJECTED: return "rejected";
        case DRIVER_FM225_RESULT_ABORTED: return "aborted";
        case DRIVER_FM225_RESULT_FAILED_CAMERA: return "camera failed";
        case DRIVER_FM225_RESULT_FAILED_UNKNOWN: return "unknown reason";
        case DRIVER_FM225_RESULT_FAILED_INVALID_PARAM: return "invalid param";
        case DRIVER_FM225_RESULT_FAILED_NO_MEMORY: return "no memory";
        case DRIVER_FM225_RESULT_FAILED_UNKNOWN_USER: return "unknown user";
        case DRIVER_FM225_RESULT_FAILED_MAX_USER: return "max user";
        case DRIVER_FM225_RESULT_FAILED_FACE_ENROLLED: return "face enrolled";
        case DRIVER_FM225_RESULT_FAILED_LIVENESS: return "liveness check failed";
        case DRIVER_FM225_RESULT_FAILED_TIMEOUT: return "timeout";
        case DRIVER_FM225_RESULT_FAILED_AUTH: return "authorization failed";
        case DRIVER_FM225_RESULT_FAILED_READ_FILE: return "read file failed";
        case DRIVER_FM225_RESULT_FAILED_WRITE_FILE: return "write file failed";
        case DRIVER_FM225_RESULT_FAILED_NO_ENCRYPT: return "encryption required";
        case DRIVER_FM225_RESULT_FAILED_NO_RGB_IMAGE: return "rgb image not ready";
        case DRIVER_FM225_RESULT_FAILED_JPG_LARGE: return "jpg too large";
        case DRIVER_FM225_RESULT_FAILED_JPG_SMALL: return "jpg too small";
        default: return "unknown";
    }
}
