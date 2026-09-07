/**
 * @file solar_calc.h
 * @brief Approximate solar position and daylight calculations for the Solar Dial face.
 */

#ifndef WATCH_OS_SOLAR_CALC_H
#define WATCH_OS_SOLAR_CALC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOLAR_CALC_STATUS_OK = 0,
    SOLAR_CALC_STATUS_POLAR_DAY,
    SOLAR_CALC_STATUS_POLAR_NIGHT,
    SOLAR_CALC_STATUS_INVALID_ARG
} solar_calc_status_t;

typedef struct {
    int year;
    int month;
    int day;
} solar_calc_date_t;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint16_t total_minutes;
} solar_calc_clock_time_t;

typedef struct {
    float latitude_deg;
    float longitude_deg;
    int32_t utc_offset_minutes;
} solar_calc_location_t;

typedef struct {
    solar_calc_clock_time_t sunrise;
    solar_calc_clock_time_t sunset;
    uint16_t daylight_minutes;
    int day_of_year;
    float declination_deg;
    float sunrise_hour_angle_deg;
    solar_calc_status_t status;
} solar_calc_day_events_t;

bool solar_calc_is_leap_year(int year);
int solar_calc_day_of_year(int year, int month, int day);
float solar_calc_declination_deg(int day_of_year);
float solar_calc_hour_angle_sunrise_deg(float latitude_deg,
                                        float declination_deg,
                                        solar_calc_status_t *status_out);
solar_calc_status_t solar_calc_calculate_sun_times(const solar_calc_location_t *location,
                                                   const solar_calc_date_t *date,
                                                   solar_calc_day_events_t *events_out);
float solar_calc_calculate_sun_angle_deg(const solar_calc_clock_time_t *current_time,
                                         const solar_calc_day_events_t *events);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_SOLAR_CALC_H */