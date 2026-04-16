#include "app_fingerprint.h"

#include "esp_log.h"

static const char *TAG = "app_fingerprint";

/* 按用户要求固定为 UART 57600, TX=GPIO17, RX=GPIO16。 */
#define AS608_UART_NUM      UART_NUM_2
#define AS608_UART_TX_GPIO  GPIO_NUM_17
#define AS608_UART_RX_GPIO  GPIO_NUM_16
#define AS608_UART_BAUD     57600

/* 与示例代码保持一致：默认搜索页范围 0~99。 */
#define AS608_DEFAULT_START_PAGE 0
#define AS608_DEFAULT_PAGE_NUM   100

static bool s_fingerprint_inited = false;

esp_err_t app_fingerprint_init(void)
{
    if (s_fingerprint_inited) {
        return ESP_OK;
    }

    const driver_as608_config_t cfg = {
        .uart_num = AS608_UART_NUM,
        .tx_gpio = AS608_UART_TX_GPIO,
        .rx_gpio = AS608_UART_RX_GPIO,
        .baud_rate = AS608_UART_BAUD,
        .addr = DRIVER_AS608_DEFAULT_ADDR,
        .cmd_timeout_ms = 2000,
    };

    esp_err_t ret = driver_as608_init(&cfg);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AS608 初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "AS608 初始化完成，固定波特率=%u", (unsigned)AS608_UART_BAUD);
    s_fingerprint_inited = true;
    return ESP_OK;
}

esp_err_t app_fingerprint_deinit(void)
{
    if (!s_fingerprint_inited) {
        return ESP_OK;
    }

    esp_err_t ret = driver_as608_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AS608 反初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    s_fingerprint_inited = false;
    return ESP_OK;
}

static esp_err_t wait_and_gen(driver_as608_char_buffer_t buf, uint8_t *confirm_code)
{
    esp_err_t ret = driver_as608_get_image(confirm_code);
    if (ret != ESP_OK) {
        return ret;
    }
    if (confirm_code != NULL && *confirm_code != DRIVER_AS608_CONFIRM_OK) {
        return ESP_OK;
    }

    return driver_as608_gen_char(buf, confirm_code);
}

esp_err_t app_fingerprint_enroll(uint16_t page_id, uint8_t *confirm_code)
{
    esp_err_t ret;

    if (!s_fingerprint_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    ret = wait_and_gen(DRIVER_AS608_CHAR_BUFFER1, confirm_code);
    if (ret != ESP_OK || (confirm_code != NULL && *confirm_code != DRIVER_AS608_CONFIRM_OK)) {
        return ret;
    }

    ret = wait_and_gen(DRIVER_AS608_CHAR_BUFFER2, confirm_code);
    if (ret != ESP_OK || (confirm_code != NULL && *confirm_code != DRIVER_AS608_CONFIRM_OK)) {
        return ret;
    }

    ret = driver_as608_match(confirm_code);
    if (ret != ESP_OK || (confirm_code != NULL && *confirm_code != DRIVER_AS608_CONFIRM_OK)) {
        return ret;
    }

    ret = driver_as608_reg_model(confirm_code);
    if (ret != ESP_OK || (confirm_code != NULL && *confirm_code != DRIVER_AS608_CONFIRM_OK)) {
        return ret;
    }

    return driver_as608_store_char(DRIVER_AS608_CHAR_BUFFER2, page_id, confirm_code);
}

esp_err_t app_fingerprint_identify(driver_as608_search_result_t *result, uint8_t *confirm_code)
{
    esp_err_t ret;

    if (!s_fingerprint_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    ret = wait_and_gen(DRIVER_AS608_CHAR_BUFFER1, confirm_code);
    if (ret != ESP_OK || (confirm_code != NULL && *confirm_code != DRIVER_AS608_CONFIRM_OK)) {
        return ret;
    }

    return driver_as608_high_speed_search(DRIVER_AS608_CHAR_BUFFER1,
                                          AS608_DEFAULT_START_PAGE,
                                          AS608_DEFAULT_PAGE_NUM,
                                          result,
                                          confirm_code);
}

esp_err_t app_fingerprint_delete(uint16_t page_id, uint8_t *confirm_code)
{
    if (!s_fingerprint_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    return driver_as608_delete_char(page_id, 1, confirm_code);
}

esp_err_t app_fingerprint_empty(uint8_t *confirm_code)
{
    if (!s_fingerprint_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    return driver_as608_empty(confirm_code);
}

esp_err_t app_fingerprint_get_valid_count(uint16_t *valid_num, uint8_t *confirm_code)
{
    if (!s_fingerprint_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    return driver_as608_valid_template_num(valid_num, confirm_code);
}

esp_err_t app_fingerprint_get_sys_para(driver_as608_sys_para_t *sys_para, uint8_t *confirm_code)
{
    if (!s_fingerprint_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    return driver_as608_read_sys_para(sys_para, confirm_code);
}

const char *app_fingerprint_confirm_message(uint8_t confirm_code)
{
    return driver_as608_confirm_message(confirm_code);
}
