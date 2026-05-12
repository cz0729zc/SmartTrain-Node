#include "attendance_profile.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "attendance_profile";

typedef struct {
    char uid[ATTENDANCE_UID_MAX_LEN];
    char student_id[ATTENDANCE_STUDENT_ID_MAX_LEN];
    char name[ATTENDANCE_NAME_MAX_LEN];
} attendance_profile_v1_t;

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

static void init_profile_defaults(attendance_profile_t *profile)
{
    profile->face_user_id = ATTENDANCE_FACE_ID_UNBOUND;
    profile->finger_page_id = ATTENDANCE_FINGER_ID_UNBOUND;
    profile->has_face_bound = false;
    profile->has_finger_bound = false;
}

static int find_index_by_uid(const char *uid)
{
    for (size_t i = 0; i < s_profile_count; i++) {
        if (strcmp(s_profiles[i].uid, uid) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int find_index_by_student_id(const char *student_id)
{
    for (size_t i = 0; i < s_profile_count; i++) {
        if (strcmp(s_profiles[i].student_id, student_id) == 0) {
            return (int)i;
        }
    }
    return -1;
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

    // 兼容旧版本结构体（仅 uid/student_id/name）
    if (required_size % sizeof(attendance_profile_t) != 0) {
        if (required_size % sizeof(attendance_profile_v1_t) == 0) {
            size_t old_count = required_size / sizeof(attendance_profile_v1_t);
            if (old_count > PROFILE_MAX_COUNT) {
                old_count = PROFILE_MAX_COUNT;
            }

            /*
             * 旧版档案缓存使用堆分配，避免在 main_task 栈上放置大数组
             * 导致日志/锁结构被破坏而触发 LoadProhibited。
             */
            attendance_profile_v1_t *old_profiles = calloc(old_count, sizeof(attendance_profile_v1_t));
            if (old_profiles == NULL) {
                ESP_LOGE(TAG, "迁移缓存分配失败");
                return ESP_ERR_NO_MEM;
            }

            size_t old_size = old_count * sizeof(attendance_profile_v1_t);
            ret = nvs_get_blob(s_nvs, PROFILE_BLOB_KEY, old_profiles, &old_size);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "读取旧版档案失败: %s", esp_err_to_name(ret));
                free(old_profiles);
                return ret;
            }

            s_profile_count = old_count;
            for (size_t i = 0; i < s_profile_count; ++i) {
                strlcpy(s_profiles[i].uid, old_profiles[i].uid, sizeof(s_profiles[i].uid));
                strlcpy(s_profiles[i].student_id, old_profiles[i].student_id, sizeof(s_profiles[i].student_id));
                strlcpy(s_profiles[i].name, old_profiles[i].name, sizeof(s_profiles[i].name));
                init_profile_defaults(&s_profiles[i]);
            }
            free(old_profiles);

            s_inited = true;
            ESP_LOGW(TAG, "检测到旧版档案，已迁移数量=%u", (unsigned)s_profile_count);
            return save_profiles_to_nvs();
        }

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

    if (uid == NULL || student_id == NULL || name == NULL || uid[0] == '\0' || student_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_index_by_uid(uid);
    if (idx >= 0) {
        strlcpy(s_profiles[idx].student_id, student_id, sizeof(s_profiles[idx].student_id));
        strlcpy(s_profiles[idx].name, name, sizeof(s_profiles[idx].name));
        ESP_LOGI(TAG, "更新档案 UID=%s 学号=%s", uid, s_profiles[idx].student_id);
        return save_profiles_to_nvs();
    }

    idx = find_index_by_student_id(student_id);
    if (idx >= 0) {
        strlcpy(s_profiles[idx].uid, uid, sizeof(s_profiles[idx].uid));
        strlcpy(s_profiles[idx].name, name, sizeof(s_profiles[idx].name));
        ESP_LOGI(TAG, "update profile student_id=%s UID=%s", student_id, s_profiles[idx].uid);
        return save_profiles_to_nvs();
    }

    if (s_profile_count >= PROFILE_MAX_COUNT) {
        ESP_LOGE(TAG, "档案容量已满");
        return ESP_ERR_NO_MEM;
    }

    strlcpy(s_profiles[s_profile_count].uid, uid, sizeof(s_profiles[s_profile_count].uid));
    strlcpy(s_profiles[s_profile_count].student_id, student_id, sizeof(s_profiles[s_profile_count].student_id));
    strlcpy(s_profiles[s_profile_count].name, name, sizeof(s_profiles[s_profile_count].name));
    init_profile_defaults(&s_profiles[s_profile_count]);
    s_profile_count++;

    ESP_LOGI(TAG, "新增档案 UID=%s 学号=%s", uid, student_id);
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

    int idx = find_index_by_uid(uid);
    if (idx >= 0) {
        *out_profile = s_profiles[idx];
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t attendance_profile_find_by_student_id(const char *student_id,
                                                attendance_profile_t *out_profile)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (student_id == NULL || out_profile == NULL || student_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_index_by_student_id(student_id);
    if (idx >= 0) {
        *out_profile = s_profiles[idx];
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t attendance_profile_find_by_face_id(uint16_t face_user_id,
                                             attendance_profile_t *out_profile)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out_profile == NULL || face_user_id == ATTENDANCE_FACE_ID_UNBOUND) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < s_profile_count; ++i) {
        if (s_profiles[i].has_face_bound && s_profiles[i].face_user_id == face_user_id) {
            *out_profile = s_profiles[i];
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t attendance_profile_find_by_finger_page(uint16_t finger_page_id,
                                                 attendance_profile_t *out_profile)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out_profile == NULL || finger_page_id == ATTENDANCE_FINGER_ID_UNBOUND) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < s_profile_count; ++i) {
        if (s_profiles[i].has_finger_bound && s_profiles[i].finger_page_id == finger_page_id) {
            *out_profile = s_profiles[i];
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t attendance_profile_bind_face_id(const char *uid, uint16_t face_user_id)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (uid == NULL || uid[0] == '\0' || face_user_id == ATTENDANCE_FACE_ID_UNBOUND) {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_index_by_uid(uid);
    if (idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    s_profiles[idx].face_user_id = face_user_id;
    s_profiles[idx].has_face_bound = true;
    return save_profiles_to_nvs();
}

esp_err_t attendance_profile_bind_finger_page(const char *uid, uint16_t finger_page_id)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (uid == NULL || uid[0] == '\0' || finger_page_id == ATTENDANCE_FINGER_ID_UNBOUND) {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_index_by_uid(uid);
    if (idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    s_profiles[idx].finger_page_id = finger_page_id;
    s_profiles[idx].has_finger_bound = true;
    return save_profiles_to_nvs();
}

esp_err_t attendance_profile_clear_biometric_bindings(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    for (size_t i = 0; i < s_profile_count; ++i) {
        s_profiles[i].face_user_id = ATTENDANCE_FACE_ID_UNBOUND;
        s_profiles[i].finger_page_id = ATTENDANCE_FINGER_ID_UNBOUND;
        s_profiles[i].has_face_bound = false;
        s_profiles[i].has_finger_bound = false;
    }

    ESP_LOGW(TAG, "cleared local biometric bindings, profiles kept=%u",
             (unsigned)s_profile_count);
    return save_profiles_to_nvs();
}

size_t attendance_profile_count(void)
{
    return s_profile_count;
}

void attendance_profile_dump(void)
{
    if (!s_inited) {
        ESP_LOGW(TAG, "profile dump skipped: not initialized");
        return;
    }

    ESP_LOGI(TAG, "profile table count=%u", (unsigned)s_profile_count);
    for (size_t i = 0; i < s_profile_count; ++i) {
        const attendance_profile_t *profile = &s_profiles[i];
        ESP_LOGI(TAG,
                 "[%u] uid=%s student_id=%s name=%s face=%s/%u finger=%s/%u",
                 (unsigned)i,
                 profile->uid,
                 profile->student_id,
                 profile->name,
                 profile->has_face_bound ? "bound" : "unbound",
                 (unsigned)profile->face_user_id,
                 profile->has_finger_bound ? "bound" : "unbound",
                 (unsigned)profile->finger_page_id);
    }
}
