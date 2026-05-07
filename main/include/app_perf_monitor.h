#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start periodic CPU runtime distribution monitor.
 *
 * This monitor prints per-task runtime distribution (delta) and highlights
 * WiFi/UI related tasks for quick bottleneck diagnosis.
 */
esp_err_t app_perf_monitor_start(void);

#ifdef __cplusplus
}
#endif

