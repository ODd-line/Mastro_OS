/**
 * @file solar_calc_test.c
 * @brief Standalone regression tests for the Solar Dial astronomy helpers.
 */

#include "astronomy/solar_calc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_TOLERANCE_MINUTES   35
#define TEST_TOLERANCE_FLOAT     0.2f

static void test_assert(bool condition, const char *message);
static void test_assert_int_close(int actual, int expected, int tolerance, const char *message);
static void test_assert_float_close(float actual, float expected, float tolerance, const char *message);
static solar_calc_clock_time_t make_time(uint8_t hour, uint8_t minute);
static void test_day_of_year(void);
static void test_declination(void);
static void test_equator_equinox(void);
static void test_mid_latitude_summer(void);
static void test_polar_cases(void);
static void test_sun_angle_mapping(void);

int main(void)
{
    test_day_of_year();
    test_declination();
    test_equator_equinox();
    test_mid_latitude_summer();
    test_polar_cases();
    test_sun_angle_mapping();

    puts("solar_calc tests passed");
    return 0;
}

static void test_assert(bool condition, const char *message)
{
    if(!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void test_assert_int_close(int actual, int expected, int tolerance, const char *message)
{
    int delta = actual - expected;

    if(delta < 0) {
        delta = -delta;
    }

    if(delta > tolerance) {
        fprintf(stderr, "FAIL: %s (actual=%d expected=%d tolerance=%d)\n", message, actual, expected, tolerance);
        exit(1);
    }
}

static void test_assert_float_close(float actual, float expected, float tolerance, const char *message)
{
    if(fabsf(actual - expected) > tolerance) {
        fprintf(stderr, "FAIL: %s (actual=%.3f expected=%.3f tolerance=%.3f)\n", message, actual, expected, tolerance);
        exit(1);
    }
}

static solar_calc_clock_time_t make_time(uint8_t hour, uint8_t minute)
{
    solar_calc_clock_time_t clock_time = {
        .hour = hour,
        .minute = minute,
        .total_minutes = (uint16_t)(((uint16_t)hour * 60U) + minute),
    };

    return clock_time;
}

static void test_day_of_year(void)
{
    test_assert(solar_calc_is_leap_year(2024), "2024 should be leap year");
    test_assert(!solar_calc_is_leap_year(2100), "2100 should not be leap year");
    test_assert_int_close(solar_calc_day_of_year(2026, 1, 1), 1, 0, "Jan 1 should be day 1");
    test_assert_int_close(solar_calc_day_of_year(2024, 3, 1), 61, 0, "Leap-year March 1 should be day 61");
    test_assert(solar_calc_day_of_year(2025, 2, 29) < 0, "Invalid non-leap date should fail");
}

static void test_declination(void)
{
    test_assert_float_close(solar_calc_declination_deg(81), 0.0f, 0.5f, "Declination near spring equinox should be near zero");
    test_assert(solar_calc_declination_deg(172) > 23.0f, "Summer solstice declination should be strongly positive");
    test_assert(solar_calc_declination_deg(355) < -23.0f, "Winter solstice declination should be strongly negative");
}

static void test_equator_equinox(void)
{
    const solar_calc_location_t location = {
        .latitude_deg = 0.0f,
        .longitude_deg = 0.0f,
        .utc_offset_minutes = 0,
    };
    const solar_calc_date_t date = {2026, 3, 22};
    solar_calc_day_events_t events;

    test_assert(solar_calc_calculate_sun_times(&location, &date, &events) == SOLAR_CALC_STATUS_OK,
                "Equator equinox calculation should succeed");
    test_assert_int_close((int)events.sunrise.total_minutes, 360, TEST_TOLERANCE_MINUTES,
                          "Equator equinox sunrise should be near 06:00");
    test_assert_int_close((int)events.sunset.total_minutes, 1080, TEST_TOLERANCE_MINUTES,
                          "Equator equinox sunset should be near 18:00");
    test_assert_int_close((int)events.daylight_minutes, 720, TEST_TOLERANCE_MINUTES,
                          "Equator equinox daylight should be near 12 hours");
}

static void test_mid_latitude_summer(void)
{
    const solar_calc_location_t location = {
        .latitude_deg = 40.7128f,
        .longitude_deg = -74.0060f,
        .utc_offset_minutes = -240,
    };
    const solar_calc_date_t date = {2026, 6, 21};
    solar_calc_day_events_t events;

    test_assert(solar_calc_calculate_sun_times(&location, &date, &events) == SOLAR_CALC_STATUS_OK,
                "NYC summer calculation should succeed");
    test_assert(events.sunrise.total_minutes < events.sunset.total_minutes,
                "NYC sunrise should occur before sunset");
    test_assert(events.sunrise.hour >= 4U && events.sunrise.hour <= 6U,
                "NYC summer sunrise should be in the early morning");
    test_assert(events.sunset.hour >= 19U && events.sunset.hour <= 21U,
                "NYC summer sunset should be in the evening");
    test_assert(events.daylight_minutes > 840U,
                "NYC summer daylight should exceed 14 hours with this approximation");
}

static void test_polar_cases(void)
{
    const solar_calc_location_t tromsø = {
        .latitude_deg = 69.6492f,
        .longitude_deg = 18.9553f,
        .utc_offset_minutes = 120,
    };
    const solar_calc_date_t midsummer = {2026, 6, 21};
    const solar_calc_date_t midwinter = {2026, 12, 21};
    const solar_calc_clock_time_t noon = {12U, 0U, 720U};
    solar_calc_day_events_t events;

    test_assert(solar_calc_calculate_sun_times(&tromsø, &midsummer, &events) == SOLAR_CALC_STATUS_POLAR_DAY,
                "Tromso midsummer should report polar day");
    test_assert(events.daylight_minutes == 1440U, "Polar day should report full daylight minutes");
    test_assert_float_close(solar_calc_calculate_sun_angle_deg(&noon, &events),
                            0.0f,
                            TEST_TOLERANCE_FLOAT,
                            "Polar day sun angle should stay at top of dial");

    test_assert(solar_calc_calculate_sun_times(&tromsø, &midwinter, &events) == SOLAR_CALC_STATUS_POLAR_NIGHT,
                "Tromso midwinter should report polar night");
    test_assert(events.daylight_minutes == 0U, "Polar night should report zero daylight minutes");
    test_assert_float_close(solar_calc_calculate_sun_angle_deg(&noon, &events),
                            180.0f,
                            TEST_TOLERANCE_FLOAT,
                            "Polar night sun angle should stay at bottom of dial");
}

static void test_sun_angle_mapping(void)
{
    const solar_calc_clock_time_t before_sunrise = {5U, 0U, 300U};
    const solar_calc_clock_time_t noon = {12U, 0U, 720U};
    const solar_calc_clock_time_t sunset = {18U, 0U, 1080U};
    solar_calc_day_events_t events = {
        .sunrise = make_time(6U, 0U),
        .sunset = make_time(18U, 0U),
        .daylight_minutes = 720U,
        .status = SOLAR_CALC_STATUS_OK,
    };

    test_assert_float_close(solar_calc_calculate_sun_angle_deg(&before_sunrise, &events),
                            225.0f,
                            TEST_TOLERANCE_FLOAT,
                            "Before sunrise the sun should clamp to the start of the dial");
    test_assert_float_close(solar_calc_calculate_sun_angle_deg(&noon, &events),
                            0.0f,
                            TEST_TOLERANCE_FLOAT,
                            "At midday the sun should be at the top of the dial");
    test_assert_float_close(solar_calc_calculate_sun_angle_deg(&sunset, &events),
                            135.0f,
                            TEST_TOLERANCE_FLOAT,
                            "At sunset the sun should reach the end of the dial");
}