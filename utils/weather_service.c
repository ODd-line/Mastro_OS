/**
 * @file weather_service.c
 * @brief Fetch and cache weather data for the watch face using Open-Meteo.
 */

#include "utils/weather_service.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "config.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "utils/wifi_manager.h"

typedef struct {
    bool initialized;
    SemaphoreHandle_t lock;
    TaskHandle_t task_handle;
    weather_service_snapshot_t snapshot;
} weather_service_context_t;

static const char *TAG = "weather_service";
static weather_service_context_t s_weather_service;

static void weather_service_task(void *task_parameter);
static esp_err_t weather_service_refresh_locked(void);
static esp_err_t weather_service_fetch_daily_forecast(int16_t *low_temperature_c,
                                                      int16_t *high_temperature_c);
static bool weather_service_snapshot_needs_refresh(time_t current_time);

esp_err_t weather_service_init(void)
{
    if(s_weather_service.initialized) {
        return ESP_OK;
    }

    memset(&s_weather_service, 0, sizeof(s_weather_service));
    s_weather_service.lock = xSemaphoreCreateMutex();
    if(s_weather_service.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if(xTaskCreate(weather_service_task,
                   "weather_service",
                   WATCH_OS_WEATHER_TASK_STACK_SIZE,
                   NULL,
                   WATCH_OS_WEATHER_TASK_PRIORITY,
                   &s_weather_service.task_handle) != pdPASS) {
        vSemaphoreDelete(s_weather_service.lock);
        memset(&s_weather_service, 0, sizeof(s_weather_service));
        return ESP_ERR_NO_MEM;
    }

    s_weather_service.initialized = true;
    return ESP_OK;
}

esp_err_t weather_service_get_snapshot(weather_service_snapshot_t *snapshot_out)
{
    if(snapshot_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(!s_weather_service.initialized || s_weather_service.lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if(xSemaphoreTake(s_weather_service.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    *snapshot_out = s_weather_service.snapshot;
    xSemaphoreGive(s_weather_service.lock);
    return ESP_OK;
}

esp_err_t weather_service_request_refresh(void)
{
    if(!s_weather_service.initialized || s_weather_service.task_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xTaskNotifyGive(s_weather_service.task_handle);
    return ESP_OK;
}

static void weather_service_task(void *task_parameter)
{
    const TickType_t wait_ticks = pdMS_TO_TICKS(WATCH_OS_WEATHER_REFRESH_INTERVAL_MS);

    (void)task_parameter;

    while(true) {
        const bool forced = (ulTaskNotifyTake(pdTRUE, wait_ticks) > 0U);
        time_t current_time;

        if(!wifi_manager_is_connected()) {
            continue;
        }

        current_time = time(NULL);
        if(!forced && !weather_service_snapshot_needs_refresh(current_time)) {
            continue;
        }

        if(xSemaphoreTake(s_weather_service.lock, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        s_weather_service.snapshot.sync_in_progress = true;
        xSemaphoreGive(s_weather_service.lock);

        (void)weather_service_refresh_locked();
    }
}

static esp_err_t weather_service_refresh_locked(void)
{
    int16_t low_temperature_c;
    int16_t high_temperature_c;
    esp_err_t ret;

    ret = weather_service_fetch_daily_forecast(&low_temperature_c, &high_temperature_c);

    if(xSemaphoreTake(s_weather_service.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if(ret == ESP_OK) {
        s_weather_service.snapshot.low_temperature_c = low_temperature_c;
        s_weather_service.snapshot.high_temperature_c = high_temperature_c;
        s_weather_service.snapshot.average_temperature_c = (int16_t)((low_temperature_c + high_temperature_c) / 2);
        s_weather_service.snapshot.updated_at = time(NULL);
        s_weather_service.snapshot.has_data = true;
    }

    s_weather_service.snapshot.sync_in_progress = false;
    xSemaphoreGive(s_weather_service.lock);
    return ret;
}

static esp_err_t weather_service_fetch_daily_forecast(int16_t *low_temperature_c,
                                                      int16_t *high_temperature_c)
{
    char url[256];
    char response_buffer[WATCH_OS_WEATHER_HTTP_BUFFER_SIZE];
    esp_http_client_config_t http_config;
    esp_http_client_handle_t client;
    cJSON *root = NULL;
    cJSON *daily = NULL;
    cJSON *max_values = NULL;
    cJSON *min_values = NULL;
    cJSON *max_today = NULL;
    cJSON *min_today = NULL;
    int total_read = 0;
    int bytes_read;
    bool is_open = false;
    esp_err_t ret = ESP_FAIL;

    if(low_temperature_c == NULL || high_temperature_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)snprintf(url,
                   sizeof(url),
                   "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto",
                   (double)WATCH_OS_DEFAULT_LATITUDE,
                   (double)WATCH_OS_DEFAULT_LONGITUDE);

    memset(&http_config, 0, sizeof(http_config));
    http_config.url = url;
    http_config.method = HTTP_METHOD_GET;
    http_config.timeout_ms = WATCH_OS_WEATHER_HTTP_TIMEOUT_MS;
    http_config.crt_bundle_attach = esp_crt_bundle_attach;
    http_config.disable_auto_redirect = true;
    http_config.skip_cert_common_name_check = false;
    http_config.user_agent = "watch_os/1.0";

    client = esp_http_client_init(&http_config);
    if(client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_http_client_open(client, 0);
    if(ret != ESP_OK) {
        goto cleanup;
    }
    is_open = true;

    (void)esp_http_client_fetch_headers(client);

    do {
        bytes_read = esp_http_client_read(client,
                                          response_buffer + total_read,
                                          (int)(sizeof(response_buffer) - 1U - (size_t)total_read));
        if(bytes_read < 0) {
            ret = ESP_FAIL;
            goto cleanup;
        }

        total_read += bytes_read;
        if((size_t)total_read >= (sizeof(response_buffer) - 1U)) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
    } while(bytes_read > 0);

    response_buffer[total_read] = '\0';
    if(esp_http_client_get_status_code(client) != 200) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    root = cJSON_Parse(response_buffer);
    if(root == NULL) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    max_values = (daily != NULL) ? cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max") : NULL;
    min_values = (daily != NULL) ? cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min") : NULL;
    max_today = cJSON_IsArray(max_values) ? cJSON_GetArrayItem(max_values, 0) : NULL;
    min_today = cJSON_IsArray(min_values) ? cJSON_GetArrayItem(min_values, 0) : NULL;
    if(!cJSON_IsNumber(max_today) || !cJSON_IsNumber(min_today)) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    if(!isfinite(max_today->valuedouble) || !isfinite(min_today->valuedouble) ||
       max_today->valuedouble < -100.0 || max_today->valuedouble > 100.0 ||
       min_today->valuedouble < -100.0 || min_today->valuedouble > 100.0) {
        ret = ESP_ERR_INVALID_RESPONSE;
        goto cleanup;
    }

    *high_temperature_c = (int16_t)((max_today->valuedouble >= 0.0) ? (max_today->valuedouble + 0.5) : (max_today->valuedouble - 0.5));
    *low_temperature_c = (int16_t)((min_today->valuedouble >= 0.0) ? (min_today->valuedouble + 0.5) : (min_today->valuedouble - 0.5));
    ret = ESP_OK;

cleanup:
    if(root != NULL) {
        cJSON_Delete(root);
    }
    if(is_open) {
        esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);
    if(ret != ESP_OK) {
        ESP_LOGW(TAG, "weather refresh failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static bool weather_service_snapshot_needs_refresh(time_t current_time)
{
    bool needs_refresh;

    if(xSemaphoreTake(s_weather_service.lock, portMAX_DELAY) != pdTRUE) {
        return true;
    }

    needs_refresh = (!s_weather_service.snapshot.has_data) ||
                    (s_weather_service.snapshot.updated_at == 0) ||
                    ((current_time - s_weather_service.snapshot.updated_at) >=
                     (time_t)(WATCH_OS_WEATHER_REFRESH_INTERVAL_MS / 1000U));

    xSemaphoreGive(s_weather_service.lock);
    return needs_refresh;
}