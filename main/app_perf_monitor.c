#include "app_perf_monitor.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_perf";

#define APP_PERF_TASK_CORE         1
#define APP_PERF_TASK_STACK        4096
#define APP_PERF_TASK_PRIO         3
#define APP_PERF_START_DELAY_MS    10000
#define APP_PERF_SAMPLE_PERIOD_MS  2000
#define APP_PERF_TOP_N             12

typedef struct {
    TaskHandle_t handle;
    uint32_t runtime;
} runtime_snapshot_t;

static TaskHandle_t s_perf_task_handle = NULL;

static bool is_focus_task_name(const char *name)
{
    if (name == NULL) {
        return false;
    }

    return (strstr(name, "wifi") != NULL) ||
           (strstr(name, "WiFi") != NULL) ||
           (strstr(name, "lvgl") != NULL) ||
           (strstr(name, "LVGL") != NULL) ||
           (strstr(name, "ui") != NULL) ||
           (strstr(name, "UI") != NULL);
}

static uint32_t find_prev_runtime(const runtime_snapshot_t *prev, UBaseType_t prev_count, TaskHandle_t handle)
{
    for (UBaseType_t i = 0; i < prev_count; ++i) {
        if (prev[i].handle == handle) {
            return prev[i].runtime;
        }
    }
    return 0;
}

static int cmp_runtime_desc(const void *a, const void *b)
{
    const TaskStatus_t *ta = (const TaskStatus_t *)a;
    const TaskStatus_t *tb = (const TaskStatus_t *)b;
    if (ta->ulRunTimeCounter < tb->ulRunTimeCounter) {
        return 1;
    }
    if (ta->ulRunTimeCounter > tb->ulRunTimeCounter) {
        return -1;
    }
    return 0;
}

static void perf_monitor_task(void *arg)
{
    (void)arg;

    runtime_snapshot_t *prev = NULL;
    UBaseType_t prev_count = 0;
    uint32_t prev_total_runtime = 0;

    /*
     * WiFi init creates high-priority tasks and allocates internal/DMA memory.
     * Avoid sampling the task list in that unstable window; the monitor is a
     * diagnostic tool and must not contend with driver bring-up.
     */
    vTaskDelay(pdMS_TO_TICKS(APP_PERF_START_DELAY_MS));

    while (1) {
        UBaseType_t task_count = uxTaskGetNumberOfTasks();
        if (task_count == 0) {
            vTaskDelay(pdMS_TO_TICKS(APP_PERF_SAMPLE_PERIOD_MS));
            continue;
        }

        TaskStatus_t *tasks = calloc(task_count, sizeof(TaskStatus_t));
        if (tasks == NULL) {
            ESP_LOGW(TAG, "calloc tasks failed, count=%u", (unsigned)task_count);
            vTaskDelay(pdMS_TO_TICKS(APP_PERF_SAMPLE_PERIOD_MS));
            continue;
        }

        uint32_t total_runtime = 0;
        UBaseType_t count = uxTaskGetSystemState(tasks, task_count, &total_runtime);
        if (count == 0 || total_runtime == 0) {
            free(tasks);
            vTaskDelay(pdMS_TO_TICKS(APP_PERF_SAMPLE_PERIOD_MS));
            continue;
        }

        uint32_t delta_total = total_runtime - prev_total_runtime;
        if (delta_total == 0 || prev == NULL || prev_count == 0) {
            runtime_snapshot_t *next_prev = calloc(count, sizeof(runtime_snapshot_t));
            if (next_prev != NULL) {
                for (UBaseType_t i = 0; i < count; ++i) {
                    next_prev[i].handle = tasks[i].xHandle;
                    next_prev[i].runtime = tasks[i].ulRunTimeCounter;
                }
                free(prev);
                prev = next_prev;
                prev_count = count;
                prev_total_runtime = total_runtime;
            }
            free(tasks);
            vTaskDelay(pdMS_TO_TICKS(APP_PERF_SAMPLE_PERIOD_MS));
            continue;
        }

        runtime_snapshot_t *curr = calloc(count, sizeof(runtime_snapshot_t));
        if (curr == NULL) {
            free(tasks);
            vTaskDelay(pdMS_TO_TICKS(APP_PERF_SAMPLE_PERIOD_MS));
            continue;
        }

        for (UBaseType_t i = 0; i < count; ++i) {
            curr[i].handle = tasks[i].xHandle;
            curr[i].runtime = tasks[i].ulRunTimeCounter;

            uint32_t prev_runtime = find_prev_runtime(prev, prev_count, tasks[i].xHandle);
            tasks[i].ulRunTimeCounter = tasks[i].ulRunTimeCounter - prev_runtime;
        }

        qsort(tasks, count, sizeof(TaskStatus_t), cmp_runtime_desc);

        uint64_t norm_total = (uint64_t)delta_total * (uint64_t)configNUM_CORES;
        ESP_LOGI(TAG, "CPU runtime delta %u us (%u ms), cores=%d, top tasks:",
                 (unsigned)delta_total,
                 (unsigned)(delta_total / 1000U),
                 configNUM_CORES);

        UBaseType_t top_n = (count < APP_PERF_TOP_N) ? count : APP_PERF_TOP_N;
        for (UBaseType_t i = 0; i < top_n; ++i) {
            const char *name = tasks[i].pcTaskName ? tasks[i].pcTaskName : "unknown";
            uint32_t run = tasks[i].ulRunTimeCounter;
            uint32_t pct_x10 = (norm_total > 0) ? (uint32_t)((run * 1000ULL) / norm_total) : 0;
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(tasks[i].xHandle);
            const char *focus = is_focus_task_name(name) ? " [focus]" : "";
            ESP_LOGI(TAG,
                     "  %-18s core=%d runtime=%8u us cpu=%3u.%u%% stack_hwm=%u%s",
                     name,
                     (int)tasks[i].xCoreID,
                     (unsigned)run,
                     (unsigned)(pct_x10 / 10U),
                     (unsigned)(pct_x10 % 10U),
                     (unsigned)hwm,
                     focus);
        }

        free(prev);
        prev = curr;
        prev_count = count;
        prev_total_runtime = total_runtime;

        free(tasks);
        vTaskDelay(pdMS_TO_TICKS(APP_PERF_SAMPLE_PERIOD_MS));
    }
}

esp_err_t app_perf_monitor_start(void)
{
    if (s_perf_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

#if CONFIG_FREERTOS_UNICORE
    BaseType_t ret = xTaskCreate(perf_monitor_task,
                                 "perf_monitor",
                                 APP_PERF_TASK_STACK,
                                 NULL,
                                 APP_PERF_TASK_PRIO,
                                 &s_perf_task_handle);
#else
    BaseType_t ret = xTaskCreatePinnedToCore(perf_monitor_task,
                                             "perf_monitor",
                                             APP_PERF_TASK_STACK,
                                             NULL,
                                             APP_PERF_TASK_PRIO,
                                             &s_perf_task_handle,
                                             APP_PERF_TASK_CORE);
#endif

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "create perf monitor task failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "perf monitor started, first sample delayed %dms, period=%dms",
             APP_PERF_START_DELAY_MS,
             APP_PERF_SAMPLE_PERIOD_MS);
    return ESP_OK;
}
