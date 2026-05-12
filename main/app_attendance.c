#include "app_attendance.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_rfid.h"
#include "app_time.h"
#include "app_wifi.h"
#include "attendance_profile.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "events_init.h"

static const char *TAG = "app_attendance";

#define ADMIN_CARD_UID "16:CE:D0:AA"
#define STUDENT_ID_BLOCK 4
#define RFID_BLOCK_SIZE 16
#define UID_BYTE_STR_LEN 3
#define CARD_WRITE_TARGET_COUNT 3

typedef enum {
    ATTENDANCE_EVENT_CARD = 0,
    ATTENDANCE_EVENT_ENTER_CARD_WRITE_MODE,
} attendance_event_type_t;

typedef struct {
    attendance_event_type_t type;
    char uid[ATTENDANCE_UID_MAX_LEN];
} attendance_event_t;

static QueueHandle_t s_event_queue = NULL;
static TaskHandle_t s_task_handle = NULL;
static char s_last_card_uid[ATTENDANCE_UID_MAX_LEN] = {0};
static bool s_card_write_mode = false;
static char s_card_write_done_uids[CARD_WRITE_TARGET_COUNT][ATTENDANCE_UID_MAX_LEN] = {0};
static size_t s_card_write_done_count = 0;

typedef struct {
    const char *uid;
    const char *student_id;
    const char *name;
} attendance_seed_profile_t;

static const attendance_seed_profile_t s_seed_profiles[] = {
    { "26:32:8E:AA", "202401001", "Student 001" },
    { "DB:85:BD:B7", "202401002", "Student 002" },
    { "CB:1D:3B:B7", "202401003", "Student 003" },
};

static esp_err_t seed_demo_profiles(void)
{
    ESP_LOGI(TAG, "admin card uid=%s", ADMIN_CARD_UID);

    for (size_t i = 0; i < (sizeof(s_seed_profiles) / sizeof(s_seed_profiles[0])); ++i) {
        const attendance_seed_profile_t *seed = &s_seed_profiles[i];
        esp_err_t ret = attendance_profile_upsert(seed->uid, seed->student_id, seed->name);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "seed profile failed uid=%s student_id=%s: %s",
                     seed->uid,
                     seed->student_id,
                     esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGI(TAG, "seed profile uid=%s student_id=%s", seed->uid, seed->student_id);
    }

    return ESP_OK;
}

static void format_uid_bytes(const uint8_t *uid, uint8_t uid_len, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }

    out[0] = '\0';
    if (uid == NULL || uid_len == 0) {
        return;
    }

    size_t pos = 0;
    for (uint8_t i = 0; i < uid_len && pos + UID_BYTE_STR_LEN < out_len; ++i) {
        int written = snprintf(out + pos,
                               out_len - pos,
                               (i == 0) ? "%02X" : ":%02X",
                               uid[i]);
        if (written <= 0 || (size_t)written >= out_len - pos) {
            out[out_len - 1] = '\0';
            return;
        }
        pos += (size_t)written;
    }
}

static void attendance_refresh_standby(void)
{
    char time_text[16] = "--:--:--";
    bool wifi_ok = app_wifi_is_connected();

    if (app_time_is_synchronized()) {
        time_t now = app_time_now();
        struct tm tm_info;
        if (localtime_r(&now, &tm_info) != NULL) {
            strftime(time_text, sizeof(time_text), "%H:%M:%S", &tm_info);
        }
    }

    events_standby_update_status(time_text, wifi_ok);
}

static bool is_valid_student_id_char(uint8_t ch)
{
    return isalnum((int)ch) || ch == '-' || ch == '_';
}

static esp_err_t read_card_student_id(char *student_id, size_t len)
{
    if (student_id == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    student_id[0] = '\0';

    uint8_t block[RFID_BLOCK_SIZE] = {0};
    esp_err_t ret = app_rfid_read_block(STUDENT_ID_BLOCK, block, sizeof(block));
    if (ret != ESP_OK) {
        return ret;
    }

    size_t out_len = 0;
    for (size_t i = 0; i < sizeof(block) && out_len < (len - 1); ++i) {
        if (block[i] == '\0' || block[i] == 0xFF) {
            break;
        }
        if (is_valid_student_id_char(block[i])) {
            student_id[out_len++] = (char)block[i];
        } else if (out_len > 0) {
            break;
        }
    }
    student_id[out_len] = '\0';

    return (out_len > 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t make_student_id_block(const char *student_id, uint8_t block[RFID_BLOCK_SIZE])
{
    if (student_id == NULL || student_id[0] == '\0' || block == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(student_id);
    if (len >= RFID_BLOCK_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(block, 0, RFID_BLOCK_SIZE);
    memcpy(block, student_id, len);
    return ESP_OK;
}

static esp_err_t write_card_student_id(const char *student_id)
{
    uint8_t write_block[RFID_BLOCK_SIZE] = {0};
    uint8_t read_after[RFID_BLOCK_SIZE] = {0};

    esp_err_t ret = make_student_id_block(student_id, write_block);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = app_rfid_read_write_verify(STUDENT_ID_BLOCK, write_block, NULL, read_after);
    if (ret != ESP_OK) {
        return ret;
    }

    if (memcmp(write_block, read_after, sizeof(write_block)) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

static void attendance_show_admin(void)
{
    events_show_admin(s_last_card_uid[0] != '\0' ? s_last_card_uid : "admin");
}

static void attendance_show_unregistered(const char *reason)
{
    events_show_unregistered_ex(s_last_card_uid, reason, EVENTS_RETURN_STANDBY);
}

static void attendance_show_admin_unregistered(const char *reason)
{
    events_show_unregistered_ex(s_last_card_uid, reason, EVENTS_RETURN_ADMIN);
}

static void attendance_show_confirm(const attendance_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }
    events_show_confirm(profile->student_id, s_last_card_uid);
}

static void attendance_enter_card_write_mode(void)
{
    s_card_write_mode = true;
    s_card_write_done_count = 0;
    memset(s_card_write_done_uids, 0, sizeof(s_card_write_done_uids));
    ESP_LOGI(TAG, "card write mode enabled");
    events_show_admin_status("Card write mode: swipe student cards");
}

static bool card_write_uid_already_done(const char *uid)
{
    for (size_t i = 0; i < s_card_write_done_count; ++i) {
        if (strcmp(s_card_write_done_uids[i], uid) == 0) {
            return true;
        }
    }
    return false;
}

static void card_write_mark_uid_done(const char *uid)
{
    if (uid == NULL || uid[0] == '\0' || card_write_uid_already_done(uid)) {
        return;
    }

    if (s_card_write_done_count < (sizeof(s_card_write_done_uids) / sizeof(s_card_write_done_uids[0]))) {
        strlcpy(s_card_write_done_uids[s_card_write_done_count],
                uid,
                sizeof(s_card_write_done_uids[s_card_write_done_count]));
        s_card_write_done_count++;
    }
}

static void attendance_handle_card_write(const char *uid)
{
    attendance_profile_t profile = {0};

    if (uid == NULL || uid[0] == '\0') {
        return;
    }

    strlcpy(s_last_card_uid, uid, sizeof(s_last_card_uid));
    ESP_LOGI(TAG, "card write mode scanned uid=%s", uid);

    if (strcmp(uid, ADMIN_CARD_UID) == 0) {
        ESP_LOGW(TAG, "skip writing admin card uid=%s", uid);
        events_show_admin_status("Admin card skipped");
        return;
    }

    if (card_write_uid_already_done(uid)) {
        events_show_admin_status("Already written");
        return;
    }

    esp_err_t ret = attendance_profile_find_by_uid(uid, &profile);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "card write rejected uid=%s: profile not found (%s)",
                 uid,
                 esp_err_to_name(ret));
        attendance_show_admin_unregistered("Unknown card");
        return;
    }

    ret = write_card_student_id(profile.student_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "card write failed uid=%s student_id=%s: %s",
                 uid,
                 profile.student_id,
                 esp_err_to_name(ret));
        events_show_admin_status("Write failed: RFID error");
        return;
    }

    ESP_LOGI(TAG,
             "card write success uid=%s student_id=%s",
             uid,
             profile.student_id);
    card_write_mark_uid_done(uid);
    char status[64] = {0};
    snprintf(status,
             sizeof(status),
             "Write OK %u/%u: %s",
             (unsigned)s_card_write_done_count,
             (unsigned)CARD_WRITE_TARGET_COUNT,
             profile.student_id);
    if (s_card_write_done_count >= CARD_WRITE_TARGET_COUNT) {
        s_card_write_mode = false;
        strlcpy(status, "Card write complete", sizeof(status));
        ESP_LOGI(TAG, "card write mode complete");
    }
    events_show_admin_status(status);
}

static void attendance_handle_card(const char *uid)
{
    attendance_profile_t profile = {0};
    char card_student_id[ATTENDANCE_STUDENT_ID_MAX_LEN] = {0};
    esp_err_t ret;

    if (uid == NULL || uid[0] == '\0') {
        return;
    }

    strlcpy(s_last_card_uid, uid, sizeof(s_last_card_uid));
    ESP_LOGI(TAG, "card scanned uid=%s", s_last_card_uid);

    if (s_card_write_mode) {
        attendance_handle_card_write(uid);
        return;
    }

    if (strcmp(uid, ADMIN_CARD_UID) == 0) {
        ESP_LOGI(TAG, "route card uid=%s -> admin", uid);
        attendance_show_admin();
        return;
    }

    ret = read_card_student_id(card_student_id, sizeof(card_student_id));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "card student_id=%s uid=%s", card_student_id, uid);
        ret = attendance_profile_find_by_student_id(card_student_id, &profile);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG,
                     "student_id not found (%s), fallback to uid=%s",
                     esp_err_to_name(ret),
                     uid);
            ret = attendance_profile_find_by_uid(uid, &profile);
        }
    } else {
        ESP_LOGW(TAG, "read card student_id failed (%s), fallback to uid=%s", esp_err_to_name(ret), uid);
        ret = attendance_profile_find_by_uid(uid, &profile);
    }

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "route card uid=%s -> unknown", uid);
        attendance_show_unregistered("Unknown card");
        return;
    }

    if (!profile.has_face_bound && !profile.has_finger_bound) {
        ESP_LOGI(TAG,
                 "route card uid=%s student_id=%s -> not registered",
                 uid,
                 profile.student_id);
        attendance_show_unregistered("Not registered");
        return;
    }

    ESP_LOGI(TAG,
             "route card uid=%s student_id=%s -> confirm",
             uid,
             profile.student_id);
    attendance_show_confirm(&profile);
}

static void attendance_rfid_cb(const rfid_card_info_t *card_info, void *arg)
{
    (void)arg;
    if (card_info == NULL) {
        return;
    }

    attendance_event_t event = {
        .type = ATTENDANCE_EVENT_CARD,
    };
    format_uid_bytes(card_info->uid, card_info->uid_length, event.uid, sizeof(event.uid));
    if (event.uid[0] == '\0') {
        strlcpy(event.uid, card_info->uid_str, sizeof(event.uid));
    }
    ESP_LOGI(TAG, "rfid callback raw_uid=%s canonical_uid=%s", card_info->uid_str, event.uid);

    if (s_event_queue != NULL) {
        xQueueSend(s_event_queue, &event, 0);
    }
}

static void attendance_task(void *arg)
{
    (void)arg;
    attendance_event_t event;
    TickType_t last_refresh = 0;

    while (1) {
        if ((xTaskGetTickCount() - last_refresh) >= pdMS_TO_TICKS(1000)) {
            attendance_refresh_standby();
            last_refresh = xTaskGetTickCount();
        }

        if (s_event_queue != NULL && xQueueReceive(s_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (event.type == ATTENDANCE_EVENT_CARD) {
                attendance_handle_card(event.uid);
            } else if (event.type == ATTENDANCE_EVENT_ENTER_CARD_WRITE_MODE) {
                attendance_enter_card_write_mode();
            }
        }
    }
}

esp_err_t app_attendance_start(void)
{
    if (s_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = attendance_profile_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = seed_demo_profiles();
    if (ret != ESP_OK) {
        return ret;
    }
    attendance_profile_dump();

    s_event_queue = xQueueCreate(8, sizeof(attendance_event_t));
    if (s_event_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    app_rfid_set_card_callback(attendance_rfid_cb, NULL);
    ret = app_rfid_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "rfid start failed: %s", esp_err_to_name(ret));
    }

    BaseType_t ok = xTaskCreatePinnedToCore(attendance_task,
                                            "attendance_task",
                                            4096,
                                            NULL,
                                            6,
                                            &s_task_handle,
                                            1);
    if (ok != pdPASS) {
        s_task_handle = NULL;
        return ESP_FAIL;
    }

    attendance_refresh_standby();
    return ESP_OK;
}

void app_attendance_handle_card_scan(const char *uid_str)
{
    attendance_handle_card(uid_str);
}

esp_err_t app_attendance_enter_card_write_mode(void)
{
    if (s_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    attendance_event_t event = {
        .type = ATTENDANCE_EVENT_ENTER_CARD_WRITE_MODE,
    };

    if (xQueueSend(s_event_queue, &event, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

bool app_attendance_is_card_write_mode(void)
{
    return s_card_write_mode;
}

esp_err_t app_attendance_exit_card_write_mode(void)
{
    s_card_write_mode = false;
    s_card_write_done_count = 0;
    memset(s_card_write_done_uids, 0, sizeof(s_card_write_done_uids));
    return ESP_OK;
}
