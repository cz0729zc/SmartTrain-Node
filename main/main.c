#include "nvs_flash.h"
#include "esp_log.h"
#include "app_wifi.h"

static const char *TAG = "main";

void app_main(void)
{
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "启动 WiFi Station 示例");
    
    // 初始化 WiFi
    app_wifi_init();
    
    // 等待连接
    if (app_wifi_wait_connected() == ESP_OK) {
        ESP_LOGI(TAG, "WiFi 已连接，开始应用程序逻辑...");
        // 此处可以放置网络相关的应用程序代码
    } else {
        ESP_LOGE(TAG, "WiFi 连接失败");
    }
}
