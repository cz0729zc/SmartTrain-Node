# Main Bootstrap Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Slim `main.c` down to a pure application entrypoint and move startup orchestration into a dedicated bootstrap module.

**Architecture:** `main.c` will keep only system bring-up, UI callback registration, and a single call into `app_bootstrap_start()`. A new `main/app_bootstrap.c` will own startup sequencing, optional boot-time cleanup, module self-check orchestration, and the transition into normal runtime. Existing feature modules stay unchanged; only the wiring moves.

**Tech Stack:** ESP-IDF v5.4.3, FreeRTOS, LVGL, existing app modules in `main/` and `components/`.

---

### Task 1: Create the bootstrap boundary

**Files:**
- Create: `main/app_bootstrap.h`
- Create: `main/app_bootstrap.c`
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Define the bootstrap API**

```c
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_bootstrap_start(void);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: Move startup sequencing into `app_bootstrap.c`**

```c
#include "app_bootstrap.h"

#include "app_attendance.h"
#include "app_face.h"
#include "app_fingerprint.h"
#include "app_lvgl.h"
#include "app_network.h"
#include "app_rfid.h"
#include "app_sensor.h"
#include "attendance_profile.h"
#include "events_init.h"
#include "gui_guider.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t app_bootstrap_start(void)
{
    /* keep the same startup order currently in app_main */
    return ESP_OK;
}
```

- [ ] **Step 3: Add the new source file to the main component**

```cmake
idf_component_register(
    SRCS
        "app_bootstrap.c"
        "app_perf_monitor.c"
        "app_face.c"
        "app_fingerprint.c"
        "app_lvgl.c"
        "app_display.c"
        "app_co2.c"
        "app_rfid.c"
        "app_time.c"
        "attendance_profile.c"
        "attendance_record.c"
        "app_attendance.c"
        "app_test_hub.c"
        "main.c"
        "app_sensor.c"
        "app_network.c"
        "sensor_data.c"
    INCLUDE_DIRS "." "include"
    REQUIRES app_wifi app_mqtt bsp common drivers ui_custom
    PRIV_REQUIRES spiffs lvgl__lvgl espressif__esp_lvgl_port abobija__rc522 nvs_flash esp_netif esp_timer esp_wifi esp_event driver
)
```

### Task 2: Trim `main.c` to entry-only wiring

**Files:**
- Modify: `main/main.c`

- [ ] **Step 1: Remove startup orchestration helpers from `main.c`**

Keep only the callback bridge code that must stay near `app_main`, and delete the embedded boot sequencing helpers that will live in `app_bootstrap.c`.

- [ ] **Step 2: Replace the long `app_main()` body with a small orchestrator**

```c
#include "app_bootstrap.h"

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(app_bootstrap_start());
}
```

- [ ] **Step 3: Preserve UI callback registration only if it still belongs in `main.c`**

Leave the UI bridge calls in one place if they are still easier to maintain there; otherwise move them behind a bootstrap helper only if the file becomes too crowded.

### Task 3: Move boot-time test and cleanup logic behind clear module boundaries

**Files:**
- Modify: `main/app_test_hub.c`
- Modify: `main/main.c`
- Modify: `main/app_bootstrap.c`

- [ ] **Step 1: Reuse `app_test_hub_run_startup_tests()` for all module probes**

```c
void app_bootstrap_startup_selfcheck(void)
{
    app_test_hub_run_startup_tests();
}
```

- [ ] **Step 2: Move `CLEAR_BIOMETRIC_DB_ON_BOOT` handling into the bootstrap module**

```c
static void clear_biometric_databases_on_boot(void);
```

- [ ] **Step 3: Keep the RF/face/fingerprint probe helpers out of `main.c`**

Convert the current helpers into `static` helpers inside `app_bootstrap.c` or route them through `app_test_hub` if they are pure diagnostics.

### Task 4: Verify the startup chain still matches the architecture doc

**Files:**
- Modify: `docs/PROJECT_ARCHITECTURE.md`

- [ ] **Step 1: Update the startup chain section**

Document that `main.c` now only initializes NVS and delegates to `app_bootstrap_start()`.

- [ ] **Step 2: Update the module responsibility table**

Mark `main.c` as entry-only and `app_bootstrap.c` as the startup orchestration layer.

- [ ] **Step 3: Confirm there is no duplicate startup logic left in docs**

Remove any stale references that still imply `main.c` owns the full boot sequence.

### Task 5: Build and sanity-check the refactor

**Files:**
- None

- [ ] **Step 1: Build locally**

Run your normal local compile flow and confirm the new `app_bootstrap` symbol resolves and the project links.

- [ ] **Step 2: Boot once and inspect logs**

Verify the startup logs still appear in the same order: NVS, UI init, self-check, attendance, network.

- [ ] **Step 3: Check for regressions in UI callbacks**

Confirm standby/admin/confirm/records buttons still reach the same handlers after the file split.

