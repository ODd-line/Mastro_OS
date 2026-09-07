/**
 * @file solar_calc.c
 * @brief Approximate solar position math for the Solar Dial face.
 */

#include "astronomy/solar_calc.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define SOLAR_CALC_MINUTES_PER_HOUR          60.0f
#define SOLAR_CALC_HOURS_PER_DAY             24.0f
#define SOLAR_CALC_MINUTES_PER_DAY           1440.0f
#define SOLAR_CALC_DEG_TO_RAD                0.01745329251994329577f
#define SOLAR_CALC_RAD_TO_DEG                57.2957795130823208768f
#define SOLAR_CALC_DIAL_START_DEG            225.0f
#define SOLAR_CALC_DIAL_SWEEP_DEG            270.0f
#define SOLAR_CALC_POLAR_DAY_ANGLE_DEG       0.0f
#define SOLAR_CALC_POLAR_NIGHT_ANGLE_DEG     180.0f

static bool solar_calc_is_valid_date(const solar_calc_date_t *date);
static bool solar_calc_is_valid_location(const solar_calc_location_t *location);
static bool solar_calc_is_valid_clock_time(const solar_calc_clock_time_t *clock_time);
static uint16_t solar_calc_total_minutes(uint8_t hour, uint8_t minute);
static void solar_calc_minutes_to_clock(float minutes_value, solar_calc_clock_time_t *clock_time);
static float solar_calc_wrap_degrees(float angle_deg);

bool solar_calc_is_leap_year(int year)
{
    if((year % 400) == 0) {
        return true;
    }

    if((year % 100) == 0) {
        return false;
    }

    return ((year % 4) == 0);
}

int solar_calc_day_of_year(int year, int month, int day)
{
    static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int day_of_year = 0;
    int month_index;
    int current_month_days;

    if(month < 1 || month > 12 || day < 1) {
        return -1;
    }

    current_month_days = days_in_month[month - 1];
    if(month == 2 && solar_calc_is_leap_year(year)) {
        current_month_days = 29;
    }

    if(day > current_month_days) {
        return -1;
    }

    for(month_index = 0; month_index < (month - 1); ++month_index) {
        day_of_year += days_in_month[month_index];
        if(month_index == 1 && solar_calc_is_leap_year(year)) {
            day_of_year += 1;
        }
    }

    return day_of_year + day;
}

float solar_calc_declination_deg(int day_of_year)
{
    if(day_of_year < 1 || day_of_year > 366) {
        return 0.0f;
    }

    return 23.45f * sinf(SOLAR_CALC_DEG_TO_RAD * (360.0f / 365.0f) * (float)(day_of_year - 81));
}

float solar_calc_hour_angle_sunrise_deg(float latitude_deg,
                                        float declination_deg,
                                        solar_calc_status_t *status_out)
{
    float cos_omega;
    solar_calc_status_t local_status = SOLAR_CALC_STATUS_OK;

    cos_omega = -tanf(SOLAR_CALC_DEG_TO_RAD * latitude_deg) * tanf(SOLAR_CALC_DEG_TO_RAD * declination_deg);

    if(cos_omega < -1.0f) {
        cos_omega = -1.0f;
        local_status = SOLAR_CALC_STATUS_POLAR_DAY;
    }
    else if(cos_omega > 1.0f) {
        cos_omega = 1.0f;
        local_status = SOLAR_CALC_STATUS_POLAR_NIGHT;
    }

    if(status_out != NULL) {
        *status_out = local_status;
    }

    return SOLAR_CALC_RAD_TO_DEG * acosf(cos_omega);
}

solar_calc_status_t solar_calc_calculate_sun_times(const solar_calc_location_t *location,
                                                   const solar_calc_date_t *date,
                                                   solar_calc_day_events_t *events_out)
{
    float declination_deg;
    float sunrise_utc_hours;
    float sunset_utc_hours;
    float utc_offset_hours;
    float solar_noon_utc_hours;
    float sunrise_local_minutes;
    float sunset_local_minutes;
    int day_of_year;
    solar_calc_status_t status = SOLAR_CALC_STATUS_OK;

    if(!solar_calc_is_valid_location(location) || !solar_calc_is_valid_date(date) || events_out == NULL) {
        return SOLAR_CALC_STATUS_INVALID_ARG;
    }

    memset(events_out, 0, sizeof(*events_out));
    day_of_year = solar_calc_day_of_year(date->year, date->month, date->day);
    if(day_of_year < 0) {
        return SOLAR_CALC_STATUS_INVALID_ARG;
    }

    declination_deg = solar_calc_declination_deg(day_of_year);
    events_out->day_of_year = day_of_year;
    events_out->declination_deg = declination_deg;
    events_out->sunrise_hour_angle_deg = solar_calc_hour_angle_sunrise_deg(location->latitude_deg,
                                                                           declination_deg,
                                                                           &status);
    events_out->status = status;

    if(status == SOLAR_CALC_STATUS_POLAR_DAY) {
        events_out->daylight_minutes = (uint16_t)SOLAR_CALC_MINUTES_PER_DAY;
        return status;
    }

    if(status == SOLAR_CALC_STATUS_POLAR_NIGHT) {
        events_out->daylight_minutes = 0U;
        return status;
    }

    solar_noon_utc_hours = 12.0f - (location->longitude_deg / 15.0f);
    sunrise_utc_hours = solar_noon_utc_hours - (events_out->sunrise_hour_angle_deg / 15.0f);
    sunset_utc_hours = solar_noon_utc_hours + (events_out->sunrise_hour_angle_deg / 15.0f);
    utc_offset_hours = (float)location->utc_offset_minutes / SOLAR_CALC_MINUTES_PER_HOUR;
    sunrise_local_minutes = (sunrise_utc_hours + utc_offset_hours) * SOLAR_CALC_MINUTES_PER_HOUR;
    sunset_local_minutes = (sunset_utc_hours + utc_offset_hours) * SOLAR_CALC_MINUTES_PER_HOUR;

    solar_calc_minutes_to_clock(sunrise_local_minutes, &events_out->sunrise);
    solar_calc_minutes_to_clock(sunset_local_minutes, &events_out->sunset);

    if(events_out->sunset.total_minutes >= events_out->sunrise.total_minutes) {
        events_out->daylight_minutes = (uint16_t)(events_out->sunset.total_minutes - events_out->sunrise.total_minutes);
    }
    else {
        events_out->daylight_minutes = (uint16_t)((uint16_t)SOLAR_CALC_MINUTES_PER_DAY -
                                                  events_out->sunrise.total_minutes +
                                                  events_out->sunset.total_minutes);
    }

    return SOLAR_CALC_STATUS_OK;
}

float solar_calc_calculate_sun_angle_deg(const solar_calc_clock_time_t *current_time,
                                         const solar_calc_day_events_t *events)
{
    float progress;

    if(!solar_calc_is_valid_clock_time(current_time) || events == NULL) {
        return SOLAR_CALC_POLAR_NIGHT_ANGLE_DEG;
    }

    if(events->status == SOLAR_CALC_STATUS_POLAR_DAY) {
        return SOLAR_CALC_POLAR_DAY_ANGLE_DEG;
    }

    if(events->status == SOLAR_CALC_STATUS_POLAR_NIGHT || events->daylight_minutes == 0U) {
        return SOLAR_CALC_POLAR_NIGHT_ANGLE_DEG;
    }

    progress = ((float)current_time->total_minutes - (float)events->sunrise.total_minutes) /
               (float)events->daylight_minutes;

    if(progress < 0.0f) {
        progress = 0.0f;
    }
    else if(progress > 1.0f) {
        progress = 1.0f;
    }

    return solar_calc_wrap_degrees(SOLAR_CALC_DIAL_START_DEG + (progress * SOLAR_CALC_DIAL_SWEEP_DEG));
}

static bool solar_calc_is_valid_date(const solar_calc_date_t *date)
{
    if(date == NULL) {
        return false;
    }

    return (solar_calc_day_of_year(date->year, date->month, date->day) > 0);
}

static bool solar_calc_is_valid_location(const solar_calc_location_t *location)
{
    if(location == NULL) {
        return false;
    }

    return (location->latitude_deg >= -90.0f && location->latitude_deg <= 90.0f &&
            location->longitude_deg >= -180.0f && location->longitude_deg <= 180.0f);
}

static bool solar_calc_is_valid_clock_time(const solar_calc_clock_time_t *clock_time)
{
    if(clock_time == NULL) {
        return false;
    }

    return (clock_time->hour < 24U && clock_time->minute < 60U &&
            clock_time->total_minutes == solar_calc_total_minutes(clock_time->hour, clock_time->minute));
}

static uint16_t solar_calc_total_minutes(uint8_t hour, uint8_t minute)
{
    return (uint16_t)(((uint16_t)hour * 60U) + minute);
}

static void solar_calc_minutes_to_clock(float minutes_value, solar_calc_clock_time_t *clock_time)
{
    float wrapped_minutes = fmodf(minutes_value, SOLAR_CALC_MINUTES_PER_DAY);
    uint16_t rounded_minutes;

    if(clock_time == NULL) {
        return;
    }

    if(wrapped_minutes < 0.0f) {
        wrapped_minutes += SOLAR_CALC_MINUTES_PER_DAY;
    }

    rounded_minutes = (uint16_t)lroundf(wrapped_minutes);
    if(rounded_minutes >= (uint16_t)SOLAR_CALC_MINUTES_PER_DAY) {
        rounded_minutes = 0U;
    }

    clock_time->hour = (uint8_t)(rounded_minutes / 60U);
    clock_time->minute = (uint8_t)(rounded_minutes % 60U);
    clock_time->total_minutes = rounded_minutes;
}

static float solar_calc_wrap_degrees(float angle_deg)
{
    float wrapped = fmodf(angle_deg, 360.0f);

    if(wrapped < 0.0f) {
        wrapped += 360.0f;
    }

    return wrapped;
}