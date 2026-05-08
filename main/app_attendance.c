#include "app_attendance.h"

#include <ctype.h>
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

#define ADMIN_CARD_UID "04:11:22:33:44"
#define STUDENT_ID_BLOCK 4
#define RFID_BLOCK_SIZE 16

typedef enum {
    ATTENDANCE_EVENT_CARD = 0,
} attendance_event_type_t;

typedef struct {
    attendance_event_type_t type;
    char uid[ATTENDANCE_UID_MAX_LEN];
} attendance_event_t;

static QueueHandle_t s_event_queue = NULL;
static TaskHandle_t s_task_handle = NULL;
static char s_last_card_uid[ATTENDANCE_UID_MAX_LEN] = {0};

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

static void attendance_show_admin(void)
{
    events_show_admin(s_last_card_uid[0] != '\0' ? s_last_card_uid : "admin");
}

static void attendance_show_unregistered(const char *reason)
{
    events_show_unregistered(s_last_card_uid, reason);
}

static void attendance_show_confirm(const attendance_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }
    events_show_confirm(profile->student_id, s_last_card_uid);
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

    if (strcmp(uid, ADMIN_CARD_UID) == 0) {
        attendance_show_admin();
        return;
    }

    ret = read_card_student_id(card_student_id, sizeof(card_student_id));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "card student_id=%s uid=%s", card_student_id, uid);
        ret = attendance_profile_find_by_student_id(card_student_id, &profile);
    } else {
        ESP_LOGW(TAG, "read card student_id failed, fallback to uid: %s", esp_err_to_name(ret));
        ret = attendance_profile_find_by_uid(uid, &profile);
    }

    if (ret != ESP_OK) {
        attendance_show_unregistered("Unknown card");
        return;
    }

    if (!profile.has_face_bound && !profile.has_finger_bound) {
        attendance_show_unregistered("Not registered");
        return;
    }

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
    strlcpy(event.uid, card_info->uid_str, sizeof(event.uid));

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
