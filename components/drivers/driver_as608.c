#include "driver_as608.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "driver_as608";

#define AS608_HEADER_H 0xEF
#define AS608_HEADER_L 0x01
#define AS608_PID_CMD  0x01
#define AS608_PID_ACK  0x07

#define AS608_CMD_GET_IMAGE         0x01
#define AS608_CMD_GEN_CHAR          0x02
#define AS608_CMD_MATCH             0x03
#define AS608_CMD_SEARCH            0x04
#define AS608_CMD_REG_MODEL         0x05
#define AS608_CMD_STORE_CHAR        0x06
#define AS608_CMD_DELETE_CHAR       0x0C
#define AS608_CMD_EMPTY             0x0D
#define AS608_CMD_READ_SYS_PARA     0x0F
#define AS608_CMD_WRITE_NOTEPAD     0x18
#define AS608_CMD_READ_NOTEPAD      0x19
#define AS608_CMD_HIGH_SPEED_SEARCH 0x1B
#define AS608_CMD_VALID_TEMPLATE    0x1D

#define AS608_MAX_PACKET_LEN 96

typedef struct {
    uint8_t payload[AS608_MAX_PACKET_LEN];
    uint16_t payload_len;
} as608_ack_t;

static bool s_inited = false;
static uart_port_t s_uart_num = UART_NUM_MAX;
static uint32_t s_addr = DRIVER_AS608_DEFAULT_ADDR;
static uint32_t s_cmd_timeout_ms = 2000;

static uint16_t calc_sum(uint8_t pid, uint16_t pack_len, const uint8_t *payload, uint16_t payload_len)
{
    uint16_t sum = pid + (pack_len >> 8) + (pack_len & 0xFF);
    for (uint16_t i = 0; i < payload_len; i++) {
        sum += payload[i];
    }
    return sum;
}

static esp_err_t read_exact(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    size_t got = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while (got < len) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }

        TickType_t wait_ticks = timeout_ticks - elapsed;
        int n = uart_read_bytes(s_uart_num, buf + got, len - got, wait_ticks);
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

static esp_err_t recv_ack(as608_ack_t *ack, uint32_t timeout_ms)
{
    uint8_t b = 0;
    uint8_t prefix[9];
    esp_err_t ret;
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while (1) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }

        ret = read_exact(&b, 1, (timeout_ticks - elapsed) * portTICK_PERIOD_MS);
        if (ret != ESP_OK) {
            return ret;
        }
        if (b == AS608_HEADER_H) {
            break;
        }
    }

    prefix[0] = AS608_HEADER_H;
    ret = read_exact(&prefix[1], sizeof(prefix) - 1, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    if (prefix[1] != AS608_HEADER_L) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint32_t addr = ((uint32_t)prefix[2] << 24) | ((uint32_t)prefix[3] << 16) |
                    ((uint32_t)prefix[4] << 8) | (uint32_t)prefix[5];
    if (addr != s_addr) {
        ESP_LOGW(TAG, "ACK 地址不匹配: 0x%08lX", (unsigned long)addr);
    }

    if (prefix[6] != AS608_PID_ACK) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint16_t pack_len = ((uint16_t)prefix[7] << 8) | prefix[8];
    if (pack_len < 3 || pack_len > AS608_MAX_PACKET_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    ret = read_exact(ack->payload, pack_len, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    uint16_t recv_sum = ((uint16_t)ack->payload[pack_len - 2] << 8) | ack->payload[pack_len - 1];
    uint16_t calc = calc_sum(AS608_PID_ACK, pack_len, ack->payload, pack_len - 2);
    if (recv_sum != calc) {
        ESP_LOGE(TAG, "ACK 校验失败 recv=0x%04X calc=0x%04X", recv_sum, calc);
        return ESP_ERR_INVALID_CRC;
    }

    ack->payload_len = pack_len - 2;
    return ESP_OK;
}

static esp_err_t send_cmd(uint8_t cmd, const uint8_t *params, uint16_t params_len)
{
    uint8_t frame[AS608_MAX_PACKET_LEN];
    uint16_t payload_len = (uint16_t)(1 + params_len);
    uint16_t pack_len = (uint16_t)(payload_len + 2);

    if (11 + params_len > AS608_MAX_PACKET_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    frame[0] = AS608_HEADER_H;
    frame[1] = AS608_HEADER_L;
    frame[2] = (uint8_t)(s_addr >> 24);
    frame[3] = (uint8_t)(s_addr >> 16);
    frame[4] = (uint8_t)(s_addr >> 8);
    frame[5] = (uint8_t)s_addr;
    frame[6] = AS608_PID_CMD;
    frame[7] = (uint8_t)(pack_len >> 8);
    frame[8] = (uint8_t)pack_len;
    frame[9] = cmd;
    if (params_len > 0 && params != NULL) {
        memcpy(&frame[10], params, params_len);
    }

    uint16_t sum = calc_sum(AS608_PID_CMD, pack_len, &frame[9], payload_len);
    frame[10 + params_len] = (uint8_t)(sum >> 8);
    frame[11 + params_len] = (uint8_t)sum;

    int n = uart_write_bytes(s_uart_num, frame, 12 + params_len);
    if (n != 12 + params_len) {
        return ESP_FAIL;
    }
    uart_wait_tx_done(s_uart_num, pdMS_TO_TICKS(100));
    return ESP_OK;
}

static esp_err_t cmd_with_ack(uint8_t cmd, const uint8_t *params, uint16_t params_len,
                              as608_ack_t *ack, uint8_t *confirm_code)
{
    esp_err_t ret;

    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    ret = send_cmd(cmd, params, params_len);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = recv_ack(ack, s_cmd_timeout_ms);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_TIMEOUT) {
            size_t pending = 0;
            (void)uart_get_buffered_data_len(s_uart_num, &pending);
            ESP_LOGW(TAG,
                     "命令 0x%02X 等待 ACK 超时，UART 缓冲区待收字节=%u (请检查 RX/TX 交叉、GND 共地、模块波特率)",
                     (unsigned)cmd,
                     (unsigned)pending);
        }
        return ret;
    }

    if (ack->payload_len < 1) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (confirm_code != NULL) {
        *confirm_code = ack->payload[0];
    }

    return ESP_OK;
}

esp_err_t driver_as608_init(const driver_as608_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    uart_config_t uart_cfg = {
        .baud_rate = (int)config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(config->uart_num, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置 UART 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(config->uart_num, config->tx_gpio, config->rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置 UART 引脚失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_driver_install(config->uart_num, 1024, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "安装 UART 驱动失败: %s", esp_err_to_name(ret));
        return ret;
    }

    s_uart_num = config->uart_num;
    s_addr = (config->addr == 0) ? DRIVER_AS608_DEFAULT_ADDR : config->addr;
    s_cmd_timeout_ms = (config->cmd_timeout_ms == 0) ? 2000 : config->cmd_timeout_ms;
    s_inited = true;

    ESP_LOGI(TAG, "AS608 初始化成功: uart=%d tx=%d rx=%d baud=%lu", (int)s_uart_num,
             (int)config->tx_gpio, (int)config->rx_gpio, (unsigned long)config->baud_rate);
    return ESP_OK;
}

esp_err_t driver_as608_deinit(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = uart_driver_delete(s_uart_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "卸载 UART 驱动失败: %s", esp_err_to_name(ret));
        return ret;
    }
    s_inited = false;
    s_uart_num = UART_NUM_MAX;
    return ESP_OK;
}

esp_err_t driver_as608_get_image(uint8_t *confirm_code)
{
    as608_ack_t ack;
    return cmd_with_ack(AS608_CMD_GET_IMAGE, NULL, 0, &ack, confirm_code);
}

esp_err_t driver_as608_gen_char(driver_as608_char_buffer_t buffer_id, uint8_t *confirm_code)
{
    as608_ack_t ack;
    uint8_t p[1] = {(uint8_t)buffer_id};
    return cmd_with_ack(AS608_CMD_GEN_CHAR, p, sizeof(p), &ack, confirm_code);
}

esp_err_t driver_as608_match(uint8_t *confirm_code)
{
    as608_ack_t ack;
    return cmd_with_ack(AS608_CMD_MATCH, NULL, 0, &ack, confirm_code);
}

esp_err_t driver_as608_search(driver_as608_char_buffer_t buffer_id, uint16_t start_page,
                              uint16_t page_num, driver_as608_search_result_t *result,
                              uint8_t *confirm_code)
{
    as608_ack_t ack;
    uint8_t p[5] = {
        (uint8_t)buffer_id,
        (uint8_t)(start_page >> 8),
        (uint8_t)start_page,
        (uint8_t)(page_num >> 8),
        (uint8_t)page_num,
    };
    esp_err_t ret = cmd_with_ack(AS608_CMD_SEARCH, p, sizeof(p), &ack, confirm_code);
    if (ret != ESP_OK) {
        return ret;
    }

    if (result != NULL && ack.payload_len >= 5) {
        result->page_id = ((uint16_t)ack.payload[1] << 8) | ack.payload[2];
        result->match_score = ((uint16_t)ack.payload[3] << 8) | ack.payload[4];
    }
    return ESP_OK;
}

esp_err_t driver_as608_reg_model(uint8_t *confirm_code)
{
    as608_ack_t ack;
    return cmd_with_ack(AS608_CMD_REG_MODEL, NULL, 0, &ack, confirm_code);
}

esp_err_t driver_as608_store_char(driver_as608_char_buffer_t buffer_id, uint16_t page_id,
                                  uint8_t *confirm_code)
{
    as608_ack_t ack;
    uint8_t p[3] = {
        (uint8_t)buffer_id,
        (uint8_t)(page_id >> 8),
        (uint8_t)page_id,
    };
    return cmd_with_ack(AS608_CMD_STORE_CHAR, p, sizeof(p), &ack, confirm_code);
}

esp_err_t driver_as608_delete_char(uint16_t page_id, uint16_t count, uint8_t *confirm_code)
{
    as608_ack_t ack;
    uint8_t p[4] = {
        (uint8_t)(page_id >> 8),
        (uint8_t)page_id,
        (uint8_t)(count >> 8),
        (uint8_t)count,
    };
    return cmd_with_ack(AS608_CMD_DELETE_CHAR, p, sizeof(p), &ack, confirm_code);
}

esp_err_t driver_as608_empty(uint8_t *confirm_code)
{
    as608_ack_t ack;
    return cmd_with_ack(AS608_CMD_EMPTY, NULL, 0, &ack, confirm_code);
}

esp_err_t driver_as608_read_sys_para(driver_as608_sys_para_t *sys_para, uint8_t *confirm_code)
{
    as608_ack_t ack;
    esp_err_t ret = cmd_with_ack(AS608_CMD_READ_SYS_PARA, NULL, 0, &ack, confirm_code);
    if (ret != ESP_OK) {
        return ret;
    }

    if (sys_para != NULL) {
        if (ack.payload_len < 17) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        sys_para->max_templates = ((uint16_t)ack.payload[5] << 8) | ack.payload[6];
        sys_para->security_level = ack.payload[8];
        sys_para->device_addr = ((uint32_t)ack.payload[9] << 24) | ((uint32_t)ack.payload[10] << 16) |
                               ((uint32_t)ack.payload[11] << 8) | (uint32_t)ack.payload[12];
        sys_para->packet_size_code = ack.payload[14];
        sys_para->baud_factor_n = ack.payload[16];
    }
    return ESP_OK;
}

esp_err_t driver_as608_high_speed_search(driver_as608_char_buffer_t buffer_id, uint16_t start_page,
                                         uint16_t page_num, driver_as608_search_result_t *result,
                                         uint8_t *confirm_code)
{
    as608_ack_t ack;
    uint8_t p[5] = {
        (uint8_t)buffer_id,
        (uint8_t)(start_page >> 8),
        (uint8_t)start_page,
        (uint8_t)(page_num >> 8),
        (uint8_t)page_num,
    };
    esp_err_t ret = cmd_with_ack(AS608_CMD_HIGH_SPEED_SEARCH, p, sizeof(p), &ack, confirm_code);
    if (ret != ESP_OK) {
        return ret;
    }

    if (result != NULL && ack.payload_len >= 5) {
        result->page_id = ((uint16_t)ack.payload[1] << 8) | ack.payload[2];
        result->match_score = ((uint16_t)ack.payload[3] << 8) | ack.payload[4];
    }
    return ESP_OK;
}

esp_err_t driver_as608_valid_template_num(uint16_t *valid_num, uint8_t *confirm_code)
{
    as608_ack_t ack;
    esp_err_t ret = cmd_with_ack(AS608_CMD_VALID_TEMPLATE, NULL, 0, &ack, confirm_code);
    if (ret != ESP_OK) {
        return ret;
    }

    if (valid_num != NULL) {
        if (ack.payload_len < 3) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        *valid_num = ((uint16_t)ack.payload[1] << 8) | ack.payload[2];
    }
    return ESP_OK;
}

esp_err_t driver_as608_write_notepad(uint8_t page_num, const uint8_t *note_32_bytes,
                                     uint8_t *confirm_code)
{
    as608_ack_t ack;
    uint8_t p[33];
    if (note_32_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    p[0] = page_num;
    memcpy(&p[1], note_32_bytes, 32);
    return cmd_with_ack(AS608_CMD_WRITE_NOTEPAD, p, sizeof(p), &ack, confirm_code);
}

esp_err_t driver_as608_read_notepad(uint8_t page_num, uint8_t *note_32_bytes,
                                    uint8_t *confirm_code)
{
    as608_ack_t ack;
    esp_err_t ret;
    uint8_t p[1] = {page_num};

    if (note_32_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = cmd_with_ack(AS608_CMD_READ_NOTEPAD, p, sizeof(p), &ack, confirm_code);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ack.payload_len < 33) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    memcpy(note_32_bytes, &ack.payload[1], 32);
    return ESP_OK;
}

const char *driver_as608_confirm_message(uint8_t c)
{
    switch (c) {
    case 0x00: return "OK";
    case 0x01: return "数据包接收错误";
    case 0x02: return "传感器上没有手指";
    case 0x03: return "录入指纹图像失败";
    case 0x04: return "指纹图像太干/太淡";
    case 0x05: return "指纹图像太湿/太糊";
    case 0x06: return "指纹图像太乱";
    case 0x07: return "特征点太少";
    case 0x08: return "指纹不匹配";
    case 0x09: return "没搜索到指纹";
    case 0x0A: return "特征合并失败";
    case 0x0B: return "地址序号超范围";
    case 0x10: return "删除模板失败";
    case 0x11: return "清空指纹库失败";
    case 0x13: return "口令不正确";
    case 0x15: return "缓冲区无有效原始图";
    case 0x18: return "读写 FLASH 出错";
    case 0x19: return "未定义错误";
    case 0x1A: return "无效寄存器号";
    case 0x1B: return "寄存器设定内容错误";
    case 0x1C: return "记事本页码错误";
    case 0x1D: return "端口操作失败";
    case 0x1E: return "自动注册失败";
    case 0x1F: return "指纹库满";
    default: return "未知确认码";
    }
}
