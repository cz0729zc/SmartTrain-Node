#include "attendance_profile.h"

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "attendance_profile";

/* NVS 命名空间与键名 */
#define PROFILE_NAMESPACE "att_profile"
#define PROFILE_BLOB_KEY  "profiles"
/* 当前实现采用内存数组 + 整体 blob 持久化，适合小规模档案 */
#define PROFILE_MAX_COUNT 128

/* 运行时缓存 */
static attendance_profile_t s_profiles[PROFILE_MAX_COUNT];
static size_t s_profile_count = 0;
static bool s_inited = false;
static nvs_handle_t s_nvs = 0;

/**
 * @brief 将内存中的档案数组整体写回 NVS
 */
static esp_err_t save_profiles_to_nvs(void)
{
    esp_err_t ret = nvs_set_blob(s_nvs, PROFILE_BLOB_KEY, s_profiles,
                                 s_profile_count * sizeof(attendance_profile_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存档案失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(s_nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "提交档案失败: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t attendance_profile_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_open(PROFILE_NAMESPACE, NVS_READWRITE, &s_nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 命名空间失败: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t required_size = 0;
    ret = nvs_get_blob(s_nvs, PROFILE_BLOB_KEY, NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        s_profile_count = 0;
        s_inited = true;
        ESP_LOGI(TAG, "档案库初始化完成，当前为空");
        return ESP_OK;
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取档案大小失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // blob 大小异常时按损坏处理并重建空库
    if (required_size % sizeof(attendance_profile_t) != 0) {
        ESP_LOGW(TAG, "档案数据损坏，已清空");
        s_profile_count = 0;
        s_inited = true;
        return save_profiles_to_nvs();
    }

    s_profile_count = required_size / sizeof(attendance_profile_t);
    if (s_profile_count > PROFILE_MAX_COUNT) {
        ESP_LOGW(TAG, "档案数量超限，截断到 %d", PROFILE_MAX_COUNT);
        s_profile_count = PROFILE_MAX_COUNT;
        required_size = s_profile_count * sizeof(attendance_profile_t);
    }

    ret = nvs_get_blob(s_nvs, PROFILE_BLOB_KEY, s_profiles, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取档案内容失败: %s", esp_err_to_name(ret));
        return ret;
    }

    s_inited = true;
    ESP_LOGI(TAG, "档案库加载完成，数量=%u", (unsigned)s_profile_count);
    return ESP_OK;
}

esp_err_t attendance_profile_upsert(const char *uid, const char *student_id, const char *name)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (uid == NULL || student_id == NULL || name == NULL || uid[0] == '\0' || student_id[0] == '\0' || name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    // 先查重，命中则更新
    for (size_t i = 0; i < s_profile_count; i++) {
        if (strcmp(s_profiles[i].uid, uid) == 0) {
            strlcpy(s_profiles[i].student_id, student_id, sizeof(s_profiles[i].student_id));
            strlcpy(s_profiles[i].name, name, sizeof(s_profiles[i].name));
            ESP_LOGI(TAG, "更新档案 UID=%s 学号=%s 姓名=%s", uid, s_profiles[i].student_id, s_profiles[i].name);
            return save_profiles_to_nvs();
        }
    }

    // 未命中则新增
    if (s_profile_count >= PROFILE_MAX_COUNT) {
        ESP_LOGE(TAG, "档案容量已满");
        return ESP_ERR_NO_MEM;
    }

    strlcpy(s_profiles[s_profile_count].uid, uid, sizeof(s_profiles[s_profile_count].uid));
    strlcpy(s_profiles[s_profile_count].student_id, student_id, sizeof(s_profiles[s_profile_count].student_id));
    strlcpy(s_profiles[s_profile_count].name, name, sizeof(s_profiles[s_profile_count].name));
    s_profile_count++;

    ESP_LOGI(TAG, "新增档案 UID=%s 学号=%s 姓名=%s", uid, student_id, name);
    return save_profiles_to_nvs();
}

esp_err_t attendance_profile_find_by_uid(const char *uid, attendance_profile_t *out_profile)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (uid == NULL || out_profile == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < s_profile_count; i++) {
        if (strcmp(s_profiles[i].uid, uid) == 0) {
            *out_profile = s_profiles[i];
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

size_t attendance_profile_count(void)
{
    return s_profile_count;
}
