#include "app_rfid.h"
#include "rc522.h"
#include "driver/rc522_i2c.h"
#include "rc522_picc.h"
#include "picc/rc522_mifare.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "app_rfid";

/* ==================== 硬件配置 ==================== */
#define RC522_I2C_ADDRESS       (0x28)       // I2C 地址 (部分模块是 0x2A)
#define RC522_I2C_ADDRESS_ALT   (0x2A)
#define RC522_I2C_SDA_GPIO      (GPIO_NUM_20) // SDA 引脚
#define RC522_I2C_SCL_GPIO      (GPIO_NUM_21) // SCL 引脚
#define RC522_RST_GPIO          (-1)          // RST 引脚 (-1 为软复位)
#define RC522_POLL_INTERVAL_MS  (125)         // 轮询间隔 (ms)
/* ================================================= */

/* RC522 句柄 */
static rc522_driver_handle_t s_driver = NULL;
static rc522_handle_t s_scanner = NULL;

/* 当前活动的 PICC (卡片) */
static rc522_picc_t *s_active_picc = NULL;
static SemaphoreHandle_t s_picc_mutex = NULL;
static SemaphoreHandle_t s_io_mutex = NULL;

/* 卡片检测回调 */
static app_rfid_card_cb_t s_card_cb = NULL;
static void *s_card_cb_arg = NULL;

/* 默认密钥 (出厂密钥) */
static const rc522_mifare_key_t s_default_key = {
    .type = RC522_MIFARE_KEY_A,
    .value = { RC522_MIFARE_KEY_VALUE_DEFAULT },
};

static esp_err_t take_io_mutex(void)
{
    if (s_io_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_io_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "RFID IO mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static esp_err_t probe_addr_once(i2c_port_t port, uint8_t addr, uint32_t timeout_ms)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(timeout_ms));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t app_rfid_probe(uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        timeout_ms = 200;
    }

    const i2c_port_t port = I2C_NUM_0;
    bool installed_by_me = false;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = RC522_I2C_SDA_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = RC522_I2C_SCL_GPIO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
        .clk_flags = 0,
    };

    esp_err_t ret = i2c_param_config(port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RFID probe i2c_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(port, conf.mode, 0, 0, 0);
    if (ret == ESP_OK) {
        installed_by_me = true;
    } else if (ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "RFID probe i2c_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ESP_ERR_TIMEOUT;
    for (int attempt = 0; attempt < 3; ++attempt) {
        ret = probe_addr_once(port, RC522_I2C_ADDRESS, timeout_ms);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "RFID probe success (addr=0x%02X, attempt=%d)", RC522_I2C_ADDRESS, attempt + 1);
            break;
        }

        esp_err_t alt_ret = probe_addr_once(port, RC522_I2C_ADDRESS_ALT, timeout_ms);
        if (alt_ret == ESP_OK) {
            ret = alt_ret;
            ESP_LOGI(TAG, "RFID probe success (addr=0x%02X, attempt=%d)", RC522_I2C_ADDRESS_ALT, attempt + 1);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    if (installed_by_me) {
        (void)i2c_driver_delete(port);
    }

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "RFID probe failed (addr=0x%02X/0x%02X): %s", RC522_I2C_ADDRESS, RC522_I2C_ADDRESS_ALT, esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief PICC 状态变化事件回调
 */
static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)event_data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        // 卡片进入感应区
        ESP_LOGI(TAG, "检测到卡片");
        rc522_picc_print(picc);

        // 保存活动卡片引用
        if (xSemaphoreTake(s_picc_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_active_picc = picc;
            xSemaphoreGive(s_picc_mutex);
        }

        // 通知上层应用
        if (s_card_cb != NULL) {
            rfid_card_info_t card_info = {0};
            card_info.uid_length = picc->uid.length;
            memcpy(card_info.uid, picc->uid.value, picc->uid.length);
            rc522_picc_uid_to_str(&picc->uid, card_info.uid_str, sizeof(card_info.uid_str));

            s_card_cb(&card_info, s_card_cb_arg);
        }
    } else if (picc->state == RC522_PICC_STATE_IDLE || picc->state == RC522_PICC_STATE_HALT) {
        // 卡片离开感应区
        ESP_LOGI(TAG, "卡片离开");

        if (xSemaphoreTake(s_picc_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_active_picc = NULL;
            xSemaphoreGive(s_picc_mutex);
        }
    }
}

esp_err_t app_rfid_start(void)
{
    if (s_scanner != NULL) {
        ESP_LOGW(TAG, "RFID 模块已启动");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "初始化 RFID 模块 (RC522 I2C)...");

    // 创建互斥锁
    s_picc_mutex = xSemaphoreCreateMutex();
    if (s_picc_mutex == NULL) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }

    s_io_mutex = xSemaphoreCreateMutex();
    if (s_io_mutex == NULL) {
        ESP_LOGE(TAG, "create RFID IO mutex failed");
        vSemaphoreDelete(s_picc_mutex);
        s_picc_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    // I2C 驱动配置
    rc522_i2c_config_t driver_config = {
        .port = I2C_NUM_0,
        .device_address = RC522_I2C_ADDRESS,
        .rw_timeout_ms = 100,
        .config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = RC522_I2C_SDA_GPIO,
            .scl_io_num = RC522_I2C_SCL_GPIO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 100000,
        },
        .rst_io_num = RC522_RST_GPIO,
    };

    // 创建 I2C 驱动
    esp_err_t ret = rc522_i2c_create(&driver_config, &s_driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 I2C 驱动失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_io_mutex);
        s_io_mutex = NULL;
        vSemaphoreDelete(s_picc_mutex);
        s_picc_mutex = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "RC522 I2C 驱动创建完成");

    // 安装驱动
    ret = rc522_driver_install(s_driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "安装驱动失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_io_mutex);
        s_io_mutex = NULL;
        vSemaphoreDelete(s_picc_mutex);
        s_picc_mutex = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "安装 RC522 驱动...");

    // 扫描器配置
    rc522_config_t scanner_config = {
        .driver = s_driver,
        .poll_interval_ms = RC522_POLL_INTERVAL_MS,
        .task_stack_size = 4096,  // RC522 内部任务栈大小
        .task_priority = 5,
        .task_mutex = s_io_mutex,
    };

    // 创建扫描器
    ESP_LOGI(TAG, "创建 RC522 扫描器...");

    ret = rc522_create(&scanner_config, &s_scanner);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建扫描器失败: %s", esp_err_to_name(ret));
        rc522_driver_uninstall(s_driver);
        vSemaphoreDelete(s_io_mutex);
        s_io_mutex = NULL;
        vSemaphoreDelete(s_picc_mutex);
        s_picc_mutex = NULL;
        return ret;
    }

    // 注册事件回调
    ret = rc522_register_events(s_scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册事件回调失败: %s", esp_err_to_name(ret));
        rc522_destroy(s_scanner);
        s_scanner = NULL;
        rc522_driver_uninstall(s_driver);
        vSemaphoreDelete(s_io_mutex);
        s_io_mutex = NULL;
        vSemaphoreDelete(s_picc_mutex);
        s_picc_mutex = NULL;
        return ret;
    }

    // 启动扫描
    ret = rc522_start(s_scanner);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动扫描失败: %s", esp_err_to_name(ret));
        rc522_destroy(s_scanner);
        s_scanner = NULL;
        rc522_driver_uninstall(s_driver);
        vSemaphoreDelete(s_io_mutex);
        s_io_mutex = NULL;
        vSemaphoreDelete(s_picc_mutex);
        s_picc_mutex = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "RFID 模块启动成功");
    ESP_LOGI(TAG, "  I2C 地址: 0x%02X", RC522_I2C_ADDRESS);
    ESP_LOGI(TAG, "  SDA: GPIO%d, SCL: GPIO%d", RC522_I2C_SDA_GPIO, RC522_I2C_SCL_GPIO);
    ESP_LOGI(TAG, "  轮询间隔: %d ms", RC522_POLL_INTERVAL_MS);

    return ESP_OK;
}

esp_err_t app_rfid_stop(void)
{
    if (s_scanner == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "停止 RFID 模块...");

    rc522_destroy(s_scanner);
    s_scanner = NULL;

    rc522_driver_uninstall(s_driver);
    s_driver = NULL;

    if (s_picc_mutex != NULL) {
        vSemaphoreDelete(s_picc_mutex);
        s_picc_mutex = NULL;
    }

    if (s_io_mutex != NULL) {
        vSemaphoreDelete(s_io_mutex);
        s_io_mutex = NULL;
    }

    s_active_picc = NULL;

    ESP_LOGI(TAG, "RFID 模块已停止");
    return ESP_OK;
}

void app_rfid_set_card_callback(app_rfid_card_cb_t callback, void *arg)
{
    s_card_cb = callback;
    s_card_cb_arg = arg;
}

esp_err_t app_rfid_read_block(uint8_t block_address, uint8_t *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size < RC522_MIFARE_BLOCK_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_scanner == NULL) {
        ESP_LOGE(TAG, "RFID 模块未启动");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_FAIL;

    ret = take_io_mutex();
    if (ret != ESP_OK) {
        return ret;
    }

    if (xSemaphoreTake(s_picc_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "获取互斥锁超时");
        xSemaphoreGive(s_io_mutex);
        return ESP_ERR_TIMEOUT;
    }

    if (s_active_picc == NULL) {
        ESP_LOGW(TAG, "没有活动的卡片");
        xSemaphoreGive(s_picc_mutex);
        xSemaphoreGive(s_io_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    // 检查是否为 MIFARE Classic 卡
    if (!rc522_mifare_type_is_classic_compatible(s_active_picc->type)) {
        ESP_LOGW(TAG, "非 MIFARE Classic 卡");
        xSemaphoreGive(s_picc_mutex);
        xSemaphoreGive(s_io_mutex);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // 认证
    ret = rc522_mifare_auth(s_scanner, s_active_picc, block_address, &s_default_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "认证失败: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_picc_mutex);
        xSemaphoreGive(s_io_mutex);
        return ret;
    }

    // 读取
    ret = rc522_mifare_read(s_scanner, s_active_picc, block_address, buffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取块 %d 失败: %s", block_address, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "读取块 %d 成功", block_address);
    }

    // 取消认证
    rc522_mifare_deauth(s_scanner, s_active_picc);

    xSemaphoreGive(s_picc_mutex);
    xSemaphoreGive(s_io_mutex);
    return ret;
}

esp_err_t app_rfid_write_block(uint8_t block_address, const uint8_t *data, size_t data_len)
{
    if (data == NULL || data_len != RC522_MIFARE_BLOCK_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    // 防止写入 Block 0 (制造商数据)
    if (block_address == 0) {
        ESP_LOGE(TAG, "禁止写入 Block 0");
        return ESP_ERR_NOT_ALLOWED;
    }

    // 警告：Sector Trailer 块
    if ((block_address + 1) % 4 == 0) {
        ESP_LOGW(TAG, "警告: 块 %d 是 Sector Trailer，写入会修改密钥/权限", block_address);
    }

    if (s_scanner == NULL) {
        ESP_LOGE(TAG, "RFID 模块未启动");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_FAIL;

    ret = take_io_mutex();
    if (ret != ESP_OK) {
        return ret;
    }

    if (xSemaphoreTake(s_picc_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "获取互斥锁超时");
        xSemaphoreGive(s_io_mutex);
        return ESP_ERR_TIMEOUT;
    }

    if (s_active_picc == NULL) {
        ESP_LOGW(TAG, "没有活动的卡片");
        xSemaphoreGive(s_picc_mutex);
        xSemaphoreGive(s_io_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    // 检查是否为 MIFARE Classic 卡
    if (!rc522_mifare_type_is_classic_compatible(s_active_picc->type)) {
        ESP_LOGW(TAG, "非 MIFARE Classic 卡");
        xSemaphoreGive(s_picc_mutex);
        xSemaphoreGive(s_io_mutex);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // 认证
    ret = rc522_mifare_auth(s_scanner, s_active_picc, block_address, &s_default_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "认证失败: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_picc_mutex);
        xSemaphoreGive(s_io_mutex);
        return ret;
    }

    // 写入
    ret = rc522_mifare_write(s_scanner, s_active_picc, block_address, data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "写入块 %d 失败: %s", block_address, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "写入块 %d 成功", block_address);
    }

    // 取消认证
    rc522_mifare_deauth(s_scanner, s_active_picc);

    xSemaphoreGive(s_picc_mutex);
    xSemaphoreGive(s_io_mutex);
    return ret;
}

esp_err_t app_rfid_read_write_verify(uint8_t block_address, const uint8_t *write_data,
                                      uint8_t *read_before, uint8_t *read_after)
{
    if (write_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 防止写入 Block 0
    if (block_address == 0) {
        ESP_LOGE(TAG, "禁止写入 Block 0");
        return ESP_ERR_NOT_ALLOWED;
    }

    // 警告 Sector Trailer
    if ((block_address + 1) % 4 == 0) {
        ESP_LOGW(TAG, "警告: 块 %d 是 Sector Trailer", block_address);
    }

    if (s_scanner == NULL) {
        ESP_LOGE(TAG, "RFID 模块未启动");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_FAIL;

    ret = take_io_mutex();
    if (ret != ESP_OK) {
        return ret;
    }

    if (xSemaphoreTake(s_picc_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "获取互斥锁超时");
        xSemaphoreGive(s_io_mutex);
        return ESP_ERR_TIMEOUT;
    }

    if (s_active_picc == NULL) {
        ESP_LOGW(TAG, "没有活动的卡片");
        xSemaphoreGive(s_picc_mutex);
        xSemaphoreGive(s_io_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    if (!rc522_mifare_type_is_classic_compatible(s_active_picc->type)) {
        ESP_LOGW(TAG, "非 MIFARE Classic 卡");
        xSemaphoreGive(s_picc_mutex);
        xSemaphoreGive(s_io_mutex);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // 1. 认证 (整个读写验证过程只认证一次)
    ret = rc522_mifare_auth(s_scanner, s_active_picc, block_address, &s_default_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "认证失败: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_picc_mutex);
        xSemaphoreGive(s_io_mutex);
        return ret;
    }

    // 2. 读取原始数据
    if (read_before != NULL) {
        ret = rc522_mifare_read(s_scanner, s_active_picc, block_address, read_before);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "读取原始数据失败: %s", esp_err_to_name(ret));
            goto cleanup;
        }
        ESP_LOGI(TAG, "读取原始数据成功");
    }

    // 3. 写入新数据
    ret = rc522_mifare_write(s_scanner, s_active_picc, block_address, write_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "写入数据失败: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ESP_LOGI(TAG, "写入数据成功");

    // 4. 读取验证
    if (read_after != NULL) {
        ret = rc522_mifare_read(s_scanner, s_active_picc, block_address, read_after);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "读取验证数据失败: %s", esp_err_to_name(ret));
            goto cleanup;
        }
        ESP_LOGI(TAG, "读取验证数据成功");
    }

cleanup:
    rc522_mifare_deauth(s_scanner, s_active_picc);
    xSemaphoreGive(s_picc_mutex);
    xSemaphoreGive(s_io_mutex);
    return ret;
}
