#include "attendance_record.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "attendance_record";

#define RECORDS_BASE_PATH "/records"
#define RECORDS_PARTITION_LABEL "records"
#define ATTENDANCE_RECORD_PATH RECORDS_BASE_PATH "/attendance.csv"
#define ATTENDANCE_RECORD_HEADER \
    "ts,time,student_id,card_uid,method,result,reason,face_user_id,finger_page_id\n"

static bool s_record_inited = false;

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
    strlcpy(item->time_text, fields[1], sizeof(item->time_text));
    strlcpy(item->student_id, fields[2], sizeof(item->student_id));
    strlcpy(item->card_uid, fields[3], sizeof(item->card_uid));
    strlcpy(item->method, fields[4], sizeof(item->method));
    strlcpy(item->result, fields[5], sizeof(item->result));
    return ESP_OK;
}

static esp_err_t ensure_record_header(void)
{
    struct stat st = {0};
    if (stat(ATTENDANCE_RECORD_PATH, &st) == 0 && st.st_size > 0) {
        return ESP_OK;
    }

    FILE *fp = fopen(ATTENDANCE_RECORD_PATH, "w");
    if (fp == NULL) {
        ESP_LOGE(TAG, "create %s failed", ATTENDANCE_RECORD_PATH);
        return ESP_FAIL;
    }

    if (fputs(ATTENDANCE_RECORD_HEADER, fp) < 0) {
        fclose(fp);
        ESP_LOGE(TAG, "write CSV header failed");
        return ESP_FAIL;
    }

    fclose(fp);
    ESP_LOGI(TAG, "created attendance record CSV: %s", ATTENDANCE_RECORD_PATH);
    return ESP_OK;
}

esp_err_t attendance_record_init(void)
{
    if (s_record_inited) {
        return ESP_OK;
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

    ret = ensure_record_header();
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

    FILE *fp = fopen(ATTENDANCE_RECORD_PATH, "a");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s append failed", ATTENDANCE_RECORD_PATH);
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
        ESP_LOGE(TAG, "append attendance record failed");
        return ESP_FAIL;
    }

    fclose(fp);
    ESP_LOGI(TAG,
             "attendance record appended student_id=%s card=%s method=%s",
             student_id,
             card_uid,
             method);
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

    FILE *fp = fopen(ATTENDANCE_RECORD_PATH, "r");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s read failed", ATTENDANCE_RECORD_PATH);
        return ESP_FAIL;
    }

    attendance_record_item_t *ring = calloc(max_items, sizeof(*ring));
    if (ring == NULL) {
        fclose(fp);
        ESP_LOGE(TAG, "allocate attendance record ring failed max_items=%u", (unsigned)max_items);
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
    return ESP_OK;
}

esp_err_t attendance_record_clear(void)
{
    if (!s_record_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    FILE *fp = fopen(ATTENDANCE_RECORD_PATH, "w");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s clear failed", ATTENDANCE_RECORD_PATH);
        return ESP_FAIL;
    }

    if (fputs(ATTENDANCE_RECORD_HEADER, fp) < 0) {
        fclose(fp);
        ESP_LOGE(TAG, "rewrite attendance record header failed");
        return ESP_FAIL;
    }

    fclose(fp);
    ESP_LOGI(TAG, "attendance records cleared path=%s", ATTENDANCE_RECORD_PATH);
    return ESP_OK;
}
