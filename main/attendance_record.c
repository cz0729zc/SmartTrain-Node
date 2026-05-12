#include "attendance_record.h"

#include <stdbool.h>
#include <stdio.h>
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
