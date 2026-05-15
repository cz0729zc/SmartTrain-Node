#include "attendance_record.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "attendance_record";

#define RECORDS_BASE_PATH "/records"
#define RECORDS_PARTITION_LABEL "records"
#define ATTENDANCE_RECORD_PATH RECORDS_BASE_PATH "/attendance.csv"
#define ATTENDANCE_PENDING_PATH RECORDS_BASE_PATH "/attendance_upload_pending.csv"
#define ATTENDANCE_PENDING_TMP_PATH RECORDS_BASE_PATH "/attendance_upload_pending.tmp"
#define ATTENDANCE_RECORD_HEADER \
    "ts,time,student_id,card_uid,method,result,reason,face_user_id,finger_page_id\n"

static bool s_record_inited = false;
static SemaphoreHandle_t s_record_mutex = NULL;

static esp_err_t record_lock(void)
{
    if (s_record_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return xSemaphoreTake(s_record_mutex, pdMS_TO_TICKS(2000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void record_unlock(void)
{
    if (s_record_mutex != NULL) {
        xSemaphoreGive(s_record_mutex);
    }
}

static void strip_line_end(char *line)
{
    if (line == NULL) {
        return;
    }

    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

static esp_err_t parse_record_line(const char *line, attendance_record_item_t *item)
{
    if (line == NULL || item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char work[192] = {0};
    strlcpy(work, line, sizeof(work));
    strip_line_end(work);

    const char *fields[9] = {0};
    char *saveptr = NULL;
    char *token = strtok_r(work, ",", &saveptr);
    size_t field_count = 0;
    while (token != NULL && field_count < (sizeof(fields) / sizeof(fields[0]))) {
        fields[field_count++] = token;
        token = strtok_r(NULL, ",", &saveptr);
    }

    if (field_count < 7) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    memset(item, 0, sizeof(*item));
    item->ts = (time_t)atoll(fields[0]);
    strlcpy(item->time_text, fields[1], sizeof(item->time_text));
    strlcpy(item->student_id, fields[2], sizeof(item->student_id));
    strlcpy(item->card_uid, fields[3], sizeof(item->card_uid));
    strlcpy(item->method, fields[4], sizeof(item->method));
    strlcpy(item->result, fields[5], sizeof(item->result));
    strlcpy(item->reason, fields[6], sizeof(item->reason));
    if (field_count > 7) {
        item->face_user_id = (uint16_t)atoi(fields[7]);
    }
    if (field_count > 8) {
        item->finger_page_id = (uint16_t)atoi(fields[8]);
    }
    return ESP_OK;
}

static esp_err_t ensure_csv_header(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == 0 && st.st_size > 0) {
        return ESP_OK;
    }

    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        ESP_LOGE(TAG, "create %s failed", path);
        return ESP_FAIL;
    }

    if (fputs(ATTENDANCE_RECORD_HEADER, fp) < 0) {
        fclose(fp);
        ESP_LOGE(TAG, "write CSV header failed");
        return ESP_FAIL;
    }

    fclose(fp);
    ESP_LOGI(TAG, "created attendance record CSV: %s", path);
    return ESP_OK;
}

static esp_err_t append_record_line(const char *path,
                                    time_t ts,
                                    const char *time_text,
                                    const char *student_id,
                                    const char *card_uid,
                                    const char *method,
                                    uint16_t face_user_id,
                                    uint16_t finger_page_id)
{
    FILE *fp = fopen(path, "a");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s append failed", path);
        return ESP_FAIL;
    }

    int written = fprintf(fp,
                          "%lld,%s,%s,%s,%s,ok,matched,%u,%u\n",
                          (long long)ts,
                          time_text,
                          student_id,
                          card_uid,
                          method,
                          (unsigned)face_user_id,
                          (unsigned)finger_page_id);
    if (written < 0) {
        fclose(fp);
        ESP_LOGE(TAG, "append attendance record failed path=%s", path);
        return ESP_FAIL;
    }

    fclose(fp);
    return ESP_OK;
}

esp_err_t attendance_record_init(void)
{
    if (s_record_inited) {
        return ESP_OK;
    }

    if (s_record_mutex == NULL) {
        s_record_mutex = xSemaphoreCreateMutex();
        if (s_record_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    const esp_vfs_spiffs_conf_t conf = {
        .base_path = RECORDS_BASE_PATH,
        .partition_label = RECORDS_PARTITION_LABEL,
        .max_files = 4,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mount records SPIFFS failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info(RECORDS_PARTITION_LABEL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "records SPIFFS mounted total=%u used=%u",
                 (unsigned)total,
                 (unsigned)used);
    } else {
        ESP_LOGW(TAG, "records SPIFFS info failed: %s", esp_err_to_name(ret));
    }

    ret = ensure_csv_header(ATTENDANCE_RECORD_PATH);
    if (ret != ESP_OK) {
        esp_vfs_spiffs_unregister(RECORDS_PARTITION_LABEL);
        return ret;
    }

    ret = ensure_csv_header(ATTENDANCE_PENDING_PATH);
    if (ret != ESP_OK) {
        esp_vfs_spiffs_unregister(RECORDS_PARTITION_LABEL);
        return ret;
    }

    s_record_inited = true;
    return ESP_OK;
}

esp_err_t attendance_record_append_success(time_t ts,
                                           const char *time_text,
                                           const char *student_id,
                                           const char *card_uid,
                                           const char *method,
                                           uint16_t face_user_id,
                                           uint16_t finger_page_id)
{
    if (!s_record_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (time_text == NULL || student_id == NULL || card_uid == NULL || method == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t lock_ret = record_lock();
    if (lock_ret != ESP_OK) {
        return lock_ret;
    }

    esp_err_t ret = append_record_line(ATTENDANCE_RECORD_PATH,
                                       ts,
                                       time_text,
                                       student_id,
                                       card_uid,
                                       method,
                                       face_user_id,
                                       finger_page_id);
    if (ret != ESP_OK) {
        record_unlock();
        return ret;
    }

    ret = append_record_line(ATTENDANCE_PENDING_PATH,
                             ts,
                             time_text,
                             student_id,
                             card_uid,
                             method,
                             face_user_id,
                             finger_page_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "append attendance pending upload failed: %s", esp_err_to_name(ret));
        record_unlock();
        return ret;
    }

    ESP_LOGI(TAG,
             "attendance record appended student_id=%s card=%s method=%s pending=%s",
             student_id,
             card_uid,
             method,
             ATTENDANCE_PENDING_PATH);
    record_unlock();
    return ESP_OK;
}

esp_err_t attendance_record_read_recent(attendance_record_item_t *items,
                                        size_t max_items,
                                        size_t *out_count)
{
    if (!s_record_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (items == NULL || max_items == 0 || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_count = 0;

    esp_err_t lock_ret = record_lock();
    if (lock_ret != ESP_OK) {
        return lock_ret;
    }

    FILE *fp = fopen(ATTENDANCE_RECORD_PATH, "r");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s read failed", ATTENDANCE_RECORD_PATH);
        record_unlock();
        return ESP_FAIL;
    }

    attendance_record_item_t *ring = calloc(max_items, sizeof(*ring));
    if (ring == NULL) {
        fclose(fp);
        ESP_LOGE(TAG, "allocate attendance record ring failed max_items=%u", (unsigned)max_items);
        record_unlock();
        return ESP_ERR_NO_MEM;
    }

    char line[192] = {0};
    size_t total_records = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "ts,", 3) == 0) {
            continue;
        }

        attendance_record_item_t parsed = {0};
        esp_err_t ret = parse_record_line(line, &parsed);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "skip bad attendance record line: %s", line);
            continue;
        }

        ring[total_records % max_items] = parsed;
        total_records++;
    }

    fclose(fp);

    size_t count = total_records < max_items ? total_records : max_items;
    for (size_t i = 0; i < count; ++i) {
        size_t newest_index = (total_records - 1U - i) % max_items;
        items[i] = ring[newest_index];
    }
    free(ring);

    *out_count = count;
    ESP_LOGI(TAG, "attendance record read recent count=%u path=%s",
             (unsigned)count,
             ATTENDANCE_RECORD_PATH);
    record_unlock();
    return ESP_OK;
}

esp_err_t attendance_record_clear(void)
{
    if (!s_record_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t lock_ret = record_lock();
    if (lock_ret != ESP_OK) {
        return lock_ret;
    }

    FILE *fp = fopen(ATTENDANCE_RECORD_PATH, "w");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s clear failed", ATTENDANCE_RECORD_PATH);
        record_unlock();
        return ESP_FAIL;
    }
    if (fputs(ATTENDANCE_RECORD_HEADER, fp) < 0) {
        fclose(fp);
        ESP_LOGE(TAG, "rewrite attendance record header failed");
        record_unlock();
        return ESP_FAIL;
    }
    fclose(fp);

    fp = fopen(ATTENDANCE_PENDING_PATH, "w");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s clear failed", ATTENDANCE_PENDING_PATH);
        record_unlock();
        return ESP_FAIL;
    }
    if (fputs(ATTENDANCE_RECORD_HEADER, fp) < 0) {
        fclose(fp);
        ESP_LOGE(TAG, "rewrite attendance pending header failed");
        record_unlock();
        return ESP_FAIL;
    }
    fclose(fp);

    ESP_LOGI(TAG,
             "attendance records cleared path=%s pending=%s",
             ATTENDANCE_RECORD_PATH,
             ATTENDANCE_PENDING_PATH);
    record_unlock();
    return ESP_OK;
}

esp_err_t attendance_record_pending_peek(attendance_record_item_t *item)
{
    if (!s_record_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t lock_ret = record_lock();
    if (lock_ret != ESP_OK) {
        return lock_ret;
    }

    FILE *fp = fopen(ATTENDANCE_PENDING_PATH, "r");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s read failed", ATTENDANCE_PENDING_PATH);
        record_unlock();
        return ESP_FAIL;
    }

    char line[192] = {0};
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "ts,", 3) == 0) {
            continue;
        }

        ret = parse_record_line(line, item);
        if (ret == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "skip bad pending attendance line: %s", line);
    }

    fclose(fp);
    record_unlock();
    return ret;
}

esp_err_t attendance_record_pending_drop_first(void)
{
    if (!s_record_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t lock_ret = record_lock();
    if (lock_ret != ESP_OK) {
        return lock_ret;
    }

    FILE *src = fopen(ATTENDANCE_PENDING_PATH, "r");
    if (src == NULL) {
        ESP_LOGE(TAG, "open %s read failed", ATTENDANCE_PENDING_PATH);
        record_unlock();
        return ESP_FAIL;
    }

    FILE *dst = fopen(ATTENDANCE_PENDING_TMP_PATH, "w");
    if (dst == NULL) {
        fclose(src);
        ESP_LOGE(TAG, "open %s write failed", ATTENDANCE_PENDING_TMP_PATH);
        record_unlock();
        return ESP_FAIL;
    }

    char line[192] = {0};
    bool dropped = false;
    while (fgets(line, sizeof(line), src) != NULL) {
        if (strncmp(line, "ts,", 3) == 0) {
            if (fputs(line, dst) < 0) {
                fclose(src);
                fclose(dst);
                record_unlock();
                return ESP_FAIL;
            }
            continue;
        }

        if (!dropped) {
            dropped = true;
            continue;
        }

        if (fputs(line, dst) < 0) {
            fclose(src);
            fclose(dst);
            ESP_LOGE(TAG, "rewrite pending queue failed");
            record_unlock();
            return ESP_FAIL;
        }
    }

    fclose(src);
    fclose(dst);

    if (!dropped) {
        remove(ATTENDANCE_PENDING_TMP_PATH);
        record_unlock();
        return ESP_ERR_NOT_FOUND;
    }

    if (remove(ATTENDANCE_PENDING_PATH) != 0) {
        ESP_LOGE(TAG, "remove %s failed", ATTENDANCE_PENDING_PATH);
        record_unlock();
        return ESP_FAIL;
    }

    if (rename(ATTENDANCE_PENDING_TMP_PATH, ATTENDANCE_PENDING_PATH) != 0) {
        ESP_LOGE(TAG, "rename pending tmp failed");
        record_unlock();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "attendance pending upload dropped first path=%s", ATTENDANCE_PENDING_PATH);
    record_unlock();
    return ESP_OK;
}
