/**
 * @file time_utils.h
 * @brief Time synchronization, formatting, and preference helpers.
 */

#ifndef WATCH_OS_TIME_UTILS_H
#define WATCH_OS_TIME_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    time_t timestamp;
    struct tm local_time;
    bool is_time_valid;
    bool is_24_hour;
    int32_t utc_offset_minutes;
} time_utils_snapshot_t;

esp_err_t time_utils_init(void);
esp_err_t time_utils_start_sync(void);
esp_err_t time_utils_wait_for_sync(uint32_t timeout_ms);
esp_err_t time_utils_force_sync(bool wait_for_sync, uint32_t timeout_ms);
esp_err_t time_utils_get_snapshot(time_utils_snapshot_t *snapshot_out);
esp_err_t time_utils_format_time(const time_utils_snapshot_t *snapshot, char *buffer, size_t buffer_size);
esp_err_t time_utils_format_date(const time_utils_snapshot_t *snapshot, char *buffer, size_t buffer_size);
bool time_utils_is_24_hour_enabled(void);
esp_err_t time_utils_set_24_hour_enabled(bool enabled);
esp_err_t time_utils_get_timezone(char *buffer, size_t buffer_size);
esp_err_t time_utils_set_timezone(const char *timezone);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_TIME_UTILS_H */