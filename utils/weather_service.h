/**
 * @file weather_service.h
 * @brief Network-backed daily weather cache for the watch face.
 */

#ifndef WATCH_OS_WEATHER_SERVICE_H
#define WATCH_OS_WEATHER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool has_data;
    bool sync_in_progress;
    int16_t low_temperature_c;
    int16_t high_temperature_c;
    int16_t average_temperature_c;
    time_t updated_at;
} weather_service_snapshot_t;

esp_err_t weather_service_init(void);
esp_err_t weather_service_get_snapshot(weather_service_snapshot_t *snapshot_out);
esp_err_t weather_service_request_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_WEATHER_SERVICE_H */