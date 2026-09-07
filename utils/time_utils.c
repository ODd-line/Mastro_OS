/**
 * @file time_utils.c
 * @brief Time synchronization, RTC-backed clock access, and formatting helpers.
 */

#include "utils/time_utils.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/hal_rtc.h"
#include "nvs.h"
#include "watch_build_time.h"

#define TIME_UTILS_NVS_TIME_FORMAT_KEY_VALUE_24H   1U
#define TIME_UTILS_NVS_TIME_FORMAT_KEY_VALUE_12H   0U

typedef struct {
    bool initialized;
    bool sync_started;
    bool is_24_hour;
    char timezone[WATCH_OS_TIMEZONE_MAX_LENGTH];
} time_utils_state_t;

static const char *TAG = "time_utils";
static time_utils_state_t s_time_utils_state;

static esp_err_t time_utils_load_preferences(void);
static esp_err_t time_utils_store_time_format(bool enabled);
static esp_err_t time_utils_store_timezone(const char *timezone);
static esp_err_t time_utils_apply_timezone(const char *timezone);
static bool time_utils_is_system_time_valid(const struct tm *time_info);
static int32_t time_utils_get_utc_offset_minutes_internal(const struct tm *time_info, time_t timestamp);
static bool time_utils_get_build_time(struct tm *time_out);
static void time_utils_restore_rtc(void);
static void time_utils_sntp_sync_cb(struct timeval *synced_time);

esp_err_t time_utils_init(void)
{
    if(s_time_utils_state.initialized) {
        return ESP_OK;
    }

    memset(&s_time_utils_state, 0, sizeof(s_time_utils_state));
    s_time_utils_state.is_24_hour = (WATCH_OS_DEFAULT_TIME_FORMAT_24H != 0);
    snprintf(s_time_utils_state.timezone, sizeof(s_time_utils_state.timezone), "%s", WATCH_OS_DEFAULT_TIMEZONE);

    ESP_RETURN_ON_ERROR(time_utils_load_preferences(), TAG, "failed to load time preferences");
    ESP_RETURN_ON_ERROR(time_utils_apply_timezone(s_time_utils_state.timezone), TAG, "failed to apply timezone");
    time_utils_restore_rtc();

    s_time_utils_state.initialized = true;
    return ESP_OK;
}

esp_err_t time_utils_start_sync(void)
{
    if(!s_time_utils_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if(s_time_utils_state.sync_started) {
        esp_sntp_stop();
        s_time_utils_state.sync_started = false;
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, WATCH_OS_NTP_SERVER_PRIMARY);
    esp_sntp_setservername(1, WATCH_OS_NTP_SERVER_SECONDARY);
    esp_sntp_set_time_sync_notification_cb(time_utils_sntp_sync_cb);
    esp_sntp_init();
    s_time_utils_state.sync_started = true;

    return ESP_OK;
}

static void time_utils_restore_rtc(void)
{
    struct tm rtc_time;
    struct tm build_time;
    bool restored_from_rtc = false;
    const bool has_build_time = time_utils_get_build_time(&build_time);

    if(hal_rtc_init() == ESP_OK && hal_rtc_get_time(&rtc_time) == ESP_OK) {
        restored_from_rtc = true;
        if(has_build_time) {
            const time_t rtc_timestamp = mktime(&rtc_time);
            const time_t build_timestamp = mktime(&build_time);
                if(rtc_timestamp != (time_t)-1 && build_timestamp != (time_t)-1 &&
                    difftime(build_timestamp, rtc_timestamp) > 1.0) {
                rtc_time = build_time;
                restored_from_rtc = false;
                ESP_LOGW(TAG, "stored RTC trails firmware build time; advancing clock");
            }
        }
    }
    else if(!has_build_time) {
        ESP_LOGW(TAG, "RTC and firmware build time are unavailable; waiting for network synchronization");
        return;
    }
    else {
        rtc_time = build_time;
    }

    const time_t timestamp = mktime(&rtc_time);
    if(timestamp == (time_t)-1) {
        ESP_LOGW(TAG, "RTC time could not be converted");
        return;
    }

    const struct timeval system_time = {
        .tv_sec = timestamp,
        .tv_usec = 0,
    };
    if(settimeofday(&system_time, NULL) == 0) {
        if(restored_from_rtc) {
            ESP_LOGI(TAG, "system clock restored from PCF85063");
        }
        else {
            ESP_LOGW(TAG, "PCF85063 was unset; initialized clock from firmware build time");
            (void)hal_rtc_set_time(&rtc_time);
        }
    }
}

static bool time_utils_get_build_time(struct tm *time_out)
{
    if(time_out == NULL) {
        return false;
    }

    const time_t build_timestamp = (time_t)WATCH_OS_BUILD_EPOCH;
    return localtime_r(&build_timestamp, time_out) != NULL;
}

static void time_utils_sntp_sync_cb(struct timeval *synced_time)
{
    struct tm local_time;

    if(synced_time == NULL) {
        return;
    }

    localtime_r(&synced_time->tv_sec, &local_time);
    if(hal_rtc_set_time(&local_time) != ESP_OK) {
        ESP_LOGW(TAG, "failed to persist synchronized time to PCF85063");
    }
}

esp_err_t time_utils_wait_for_sync(uint32_t timeout_ms)
{
    TickType_t start_ticks;
    struct tm current_time_info;
    time_t current_timestamp;

    if(!s_time_utils_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    start_ticks = xTaskGetTickCount();
    do {
        current_timestamp = time(NULL);
        localtime_r(&current_timestamp, &current_time_info);
        if(time_utils_is_system_time_valid(&current_time_info)) {
            return ESP_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(WATCH_OS_NTP_SYNC_POLL_MS));
    } while(((xTaskGetTickCount() - start_ticks) * portTICK_PERIOD_MS) < timeout_ms);

    return ESP_ERR_TIMEOUT;
}

esp_err_t time_utils_force_sync(bool wait_for_sync, uint32_t timeout_ms)
{
    ESP_RETURN_ON_ERROR(time_utils_start_sync(), TAG, "failed to start sntp sync");
    if(!wait_for_sync) {
        return ESP_OK;
    }

    return time_utils_wait_for_sync(timeout_ms);
}

esp_err_t time_utils_get_snapshot(time_utils_snapshot_t *snapshot_out)
{
    struct tm current_time_info;
    time_t current_timestamp;

    if(snapshot_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(!s_time_utils_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(snapshot_out, 0, sizeof(*snapshot_out));
    current_timestamp = time(NULL);
    localtime_r(&current_timestamp, &current_time_info);

    snapshot_out->timestamp = current_timestamp;
    snapshot_out->local_time = current_time_info;
    snapshot_out->is_time_valid = time_utils_is_system_time_valid(&current_time_info);
    snapshot_out->is_24_hour = s_time_utils_state.is_24_hour;
    snapshot_out->utc_offset_minutes = time_utils_get_utc_offset_minutes_internal(&current_time_info, current_timestamp);
    return ESP_OK;
}

esp_err_t time_utils_format_time(const time_utils_snapshot_t *snapshot, char *buffer, size_t buffer_size)
{
    const char *format_string;

    if(snapshot == NULL || buffer == NULL || buffer_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if(!snapshot->is_time_valid) {
        return ESP_ERR_INVALID_STATE;
    }

    format_string = snapshot->is_24_hour ? "%H:%M" : "%I:%M";
    if(strftime(buffer, buffer_size, format_string, &snapshot->local_time) == 0U) {
        return ESP_ERR_INVALID_SIZE;
    }

    if(!snapshot->is_24_hour && buffer[0] == '0') {
        memmove(buffer, buffer + 1, strlen(buffer));
    }

    return ESP_OK;
}

esp_err_t time_utils_format_date(const time_utils_snapshot_t *snapshot, char *buffer, size_t buffer_size)
{
    if(snapshot == NULL || buffer == NULL || buffer_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if(!snapshot->is_time_valid) {
        return ESP_ERR_INVALID_STATE;
    }

    if(strftime(buffer, buffer_size, "%a %d %b", &snapshot->local_time) == 0U) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

bool time_utils_is_24_hour_enabled(void)
{
    return s_time_utils_state.is_24_hour;
}

esp_err_t time_utils_set_24_hour_enabled(bool enabled)
{
    if(!s_time_utils_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(time_utils_store_time_format(enabled), TAG, "failed to store time format");
    s_time_utils_state.is_24_hour = enabled;
    return ESP_OK;
}

esp_err_t time_utils_get_timezone(char *buffer, size_t buffer_size)
{
    if(buffer == NULL || buffer_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if(!s_time_utils_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    snprintf(buffer, buffer_size, "%s", s_time_utils_state.timezone);
    return ESP_OK;
}

esp_err_t time_utils_set_timezone(const char *timezone)
{
    if(timezone == NULL || timezone[0] == '\0' ||
       strnlen(timezone, WATCH_OS_TIMEZONE_MAX_LENGTH) >= WATCH_OS_TIMEZONE_MAX_LENGTH) {
        return ESP_ERR_INVALID_ARG;
    }

    if(!s_time_utils_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(time_utils_store_timezone(timezone), TAG, "failed to store timezone");
    snprintf(s_time_utils_state.timezone, sizeof(s_time_utils_state.timezone), "%s", timezone);
    return time_utils_apply_timezone(s_time_utils_state.timezone);
}

static esp_err_t time_utils_load_preferences(void)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;
    size_t value_size = sizeof(s_time_utils_state.timezone);
    uint8_t time_format_storage_value = TIME_UTILS_NVS_TIME_FORMAT_KEY_VALUE_24H;

    ret = nvs_open(WATCH_OS_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if(ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if(ret != ESP_OK) {
        return ret;
    }

    ret = nvs_get_u8(nvs_handle, WATCH_OS_NVS_KEY_TIME_FORMAT, &time_format_storage_value);
    if(ret == ESP_OK) {
        s_time_utils_state.is_24_hour = (time_format_storage_value == TIME_UTILS_NVS_TIME_FORMAT_KEY_VALUE_24H);
    }
    else if(ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_get_str(nvs_handle, WATCH_OS_NVS_KEY_TIMEZONE, s_time_utils_state.timezone, &value_size);
    if(ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs_handle);
        return ret;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

static esp_err_t time_utils_store_time_format(bool enabled)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;
    const uint8_t time_format_storage_value = enabled ? TIME_UTILS_NVS_TIME_FORMAT_KEY_VALUE_24H :
                                                        TIME_UTILS_NVS_TIME_FORMAT_KEY_VALUE_12H;

    ESP_RETURN_ON_ERROR(nvs_open(WATCH_OS_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle),
                        TAG,
                        "nvs_open failed for time format");

    ret = nvs_set_u8(nvs_handle, WATCH_OS_NVS_KEY_TIME_FORMAT, time_format_storage_value);
    if(ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return ret;
}

static esp_err_t time_utils_store_timezone(const char *timezone)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;

    ESP_RETURN_ON_ERROR(nvs_open(WATCH_OS_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle),
                        TAG,
                        "nvs_open failed for timezone");

    ret = nvs_set_str(nvs_handle, WATCH_OS_NVS_KEY_TIMEZONE, timezone);
    if(ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return ret;
}

static esp_err_t time_utils_apply_timezone(const char *timezone)
{
    if(timezone == NULL || timezone[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if(setenv("TZ", timezone, 1) != 0) {
        return ESP_FAIL;
    }

    tzset();
    return ESP_OK;
}

static bool time_utils_is_system_time_valid(const struct tm *time_info)
{
    if(time_info == NULL) {
        return false;
    }

    return ((time_info->tm_year + 1900) >= WATCH_OS_MIN_VALID_YEAR);
}

static int32_t time_utils_get_utc_offset_minutes_internal(const struct tm *time_info, time_t timestamp)
{
    struct tm utc_time_info;
    time_t local_epoch;
    time_t utc_epoch;
    struct tm *utc_time_ptr;

    if(time_info == NULL) {
        return 0;
    }

    utc_time_ptr = gmtime(&timestamp);
    if(utc_time_ptr == NULL) {
        return 0;
    }

    utc_time_info = *utc_time_ptr;
    local_epoch = mktime((struct tm *)time_info);
    utc_epoch = mktime(&utc_time_info);
    return (int32_t)((local_epoch - utc_epoch) / 60);
}