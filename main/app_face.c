#include "app_face.h"

#include "esp_log.h"

static const char *TAG = "app_face";
static bool s_face_inited = false;

esp_err_t app_face_init(const app_face_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_face_inited) {
        return ESP_OK;
    }

    driver_fm225_config_t cfg = {
        .uart_num = config->uart_num,
        .tx_gpio = config->tx_gpio,
        .rx_gpio = config->rx_gpio,
        .baud_rate = config->baud_rate,
        .cmd_timeout_ms = config->cmd_timeout_ms,
        .rx_buffer_size = 1024,
    };

    esp_err_t ret = driver_fm225_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FM225 初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    s_face_inited = true;
    ESP_LOGI(TAG, "FM225 初始化完成");
    return ESP_OK;
}

esp_err_t app_face_deinit(void)
{
    if (!s_face_inited) {
        return ESP_OK;
    }

    esp_err_t ret = driver_fm225_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FM225 反初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    s_face_inited = false;
    return ESP_OK;
}

esp_err_t app_face_wait_ready(uint32_t timeout_ms)
{
    if (!s_face_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    return driver_fm225_wait_ready(timeout_ms);
}

esp_err_t app_face_get_status(driver_fm225_status_t *status, uint8_t *result_code)
{
    if (!s_face_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    return driver_fm225_get_status(status, result_code);
}

esp_err_t app_face_verify_once(uint8_t timeout_s, driver_fm225_verify_result_t *verify_result,
                               uint8_t *result_code)
{
    if (!s_face_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    return driver_fm225_verify(0, timeout_s, verify_result, result_code);
}

esp_err_t app_face_enroll_single(const char *user_name, bool admin, uint8_t timeout_s,
                                 uint16_t *user_id, uint8_t *result_code)
{
    if (!s_face_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t face_dir_bitmap = 0;
    esp_err_t ret = driver_fm225_enroll_itg(admin ? 1U : 0U,
                                            user_name,
                                            DRIVER_FM225_ENROLL_SINGLE,
                                            0,
                                            timeout_s,
                                            user_id,
                                            &face_dir_bitmap,
                                            result_code);
    if (ret == ESP_OK && result_code != NULL && *result_code == DRIVER_FM225_RESULT_SUCCESS) {
        ESP_LOGI(TAG, "FM225 单帧录入成功, user_id=%u, face_dir_bitmap=0x%02X",
                 user_id ? (unsigned)(*user_id) : 0U,
                 face_dir_bitmap);
    }

    return ret;
}

esp_err_t app_face_delete_all(uint8_t *result_code)
{
    if (!s_face_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    return driver_fm225_delete_all(result_code);
}

const char *app_face_result_message(uint8_t result_code)
{
    return driver_fm225_result_message(result_code);
}
