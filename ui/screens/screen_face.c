/**
 * @file screen_face.c
 * @brief Solar Dial watch face implementation.
 */

#include "ui/screens/screen_face.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "astronomy/solar_calc.h"
#include "config.h"
#include "esp_check.h"
#include "esp_err.h"
#include "ui/ui_defs.h"
#include "utils/time_utils.h"
#include "utils/weather_service.h"

#define SCREEN_FACE_DATE_BUFFER_SIZE          32U
#define SCREEN_FACE_TIME_BUFFER_SIZE          8U
#define SCREEN_FACE_COMPLICATION_BUFFER_SIZE  16U
#define SCREEN_FACE_MINUTES_PER_DAY           1440U

typedef enum {
    SCREEN_FACE_PHASE_MORNING = 0,
    SCREEN_FACE_PHASE_DAY,
    SCREEN_FACE_PHASE_EVENING,
    SCREEN_FACE_PHASE_NIGHT
} screen_face_phase_t;

typedef struct {
    bool initialized;
    int cached_day_of_year;
    int32_t cached_utc_offset_minutes;
    solar_calc_day_events_t solar_events;
    lv_obj_t *root;
    lv_obj_t *label_time;
    lv_obj_t *label_date;
    lv_obj_t *label_day;
    lv_obj_t *label_ring_legend;
    lv_obj_t *time_ring;
    lv_obj_t *tectonic_ring;
    lv_obj_t *solar_ring;
    lv_obj_t *solar_orb;
    lv_obj_t *hour_markers[UI_SOLAR_DIAL_CARDINAL_MARKER_COUNT];
    lv_obj_t *complication_sunrise;
    lv_obj_t *complication_weather_low;
    lv_obj_t *complication_weather_high;
    lv_obj_t *label_sunrise_value;
    lv_obj_t *label_weather_low_value;
    lv_obj_t *label_weather_high_value;
    lv_obj_t *label_weather_average_value;
    lv_timer_t *clock_timer;
} screen_face_state_t;

static screen_face_state_t s_face_state;

static esp_err_t screen_face_build_layout(void);
static esp_err_t screen_face_refresh_day_events(const time_utils_snapshot_t *time_snapshot);
static screen_face_phase_t screen_face_get_phase(uint16_t current_total_minutes,
                                                 const solar_calc_day_events_t *solar_events);
static void screen_face_apply_visuals(screen_face_phase_t phase,
                                      uint16_t current_total_minutes,
                                      const solar_calc_day_events_t *solar_events,
                                      const time_utils_snapshot_t *time_snapshot);
static lv_obj_t *screen_face_create_ring_arc(lv_obj_t *parent, lv_coord_t size, lv_coord_t width);
static esp_err_t screen_face_create_cardinal_markers(void);
static lv_obj_t *screen_face_create_complication(lv_obj_t *parent,
                                                 lv_align_t align,
                                                 lv_coord_t x_offset,
                                                 lv_coord_t y_offset,
                                                 bool right_aligned,
                                                 lv_color_t icon_color,
                                                 lv_obj_t **value_label);
static void screen_face_position_on_dial(lv_obj_t *object, lv_coord_t size_px, float dial_angle_deg, lv_coord_t radius_px);
static float screen_face_clock_minutes_to_ring_angle_deg(uint16_t total_minutes);
static lv_coord_t screen_face_angle_to_lv_deg(float angle_deg);
static void screen_face_format_solar_time(const solar_calc_clock_time_t *clock_time,
                                          bool is_24_hour,
                                          char *buffer,
                                          size_t buffer_size);
static uint8_t screen_face_get_demo_plate_angle_deg(const time_utils_snapshot_t *time_snapshot);
static void screen_face_clock_timer_cb(lv_timer_t *timer);

/** {@inheritDoc screen_face_init} */
esp_err_t screen_face_init(void)
{
    if(s_face_state.initialized) {
        screen_face_refresh();
        return ESP_OK;
    }

    memset(&s_face_state, 0, sizeof(s_face_state));
    return screen_face_build_layout();
}

/** {@inheritDoc screen_face_get_root} */
lv_obj_t *screen_face_get_root(void)
{
    return s_face_state.root;
}

/** {@inheritDoc screen_face_refresh} */
void screen_face_refresh(void)
{
    time_utils_snapshot_t time_snapshot;
    solar_calc_clock_time_t current_clock_time;
    char time_buffer[SCREEN_FACE_TIME_BUFFER_SIZE];
    char date_buffer[SCREEN_FACE_DATE_BUFFER_SIZE];
    char sunrise_buffer[SCREEN_FACE_COMPLICATION_BUFFER_SIZE];
    weather_service_snapshot_t weather_snapshot;
    bool has_weather_data = false;
    uint16_t current_total_minutes;
    screen_face_phase_t phase;

    if(s_face_state.label_time == NULL || s_face_state.label_date == NULL) {
        return;
    }

    if(time_utils_get_snapshot(&time_snapshot) != ESP_OK || !time_snapshot.is_time_valid) {
        lv_label_set_text(s_face_state.label_time, "--:--");
        lv_label_set_text(s_face_state.label_date, "Time sync pending");
        if(s_face_state.label_sunrise_value != NULL) {
            lv_label_set_text(s_face_state.label_sunrise_value, "--:--");
        }
        return;
    }

    if(time_utils_format_time(&time_snapshot, time_buffer, sizeof(time_buffer)) != ESP_OK) {
        lv_label_set_text(s_face_state.label_time, "--:--");
        return;
    }

    static const char *month_names[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    (void)snprintf(date_buffer,
                   sizeof(date_buffer),
                   "%02d %s",
                   time_snapshot.local_time.tm_mday,
                   month_names[time_snapshot.local_time.tm_mon]);

    ESP_RETURN_VOID_ON_ERROR(screen_face_refresh_day_events(&time_snapshot), "screen_face", "solar event refresh failed");
    has_weather_data = (weather_service_get_snapshot(&weather_snapshot) == ESP_OK) && weather_snapshot.has_data;

    current_clock_time.hour = (uint8_t)time_snapshot.local_time.tm_hour;
    current_clock_time.minute = (uint8_t)time_snapshot.local_time.tm_min;
    current_total_minutes = (uint16_t)((current_clock_time.hour * 60U) + current_clock_time.minute);
    current_clock_time.total_minutes = current_total_minutes;
    phase = screen_face_get_phase(current_total_minutes, &s_face_state.solar_events);

    lv_label_set_text(s_face_state.label_time, time_buffer);
    lv_label_set_text(s_face_state.label_date, date_buffer);
    if(s_face_state.label_day != NULL) {
        static const char *day_names[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        lv_label_set_text(s_face_state.label_day, day_names[time_snapshot.local_time.tm_wday]);
    }

    if(s_face_state.solar_events.status == SOLAR_CALC_STATUS_POLAR_DAY) {
        (void)snprintf(sunrise_buffer, sizeof(sunrise_buffer), "%s", "All day");
    }
    else if(s_face_state.solar_events.status == SOLAR_CALC_STATUS_POLAR_NIGHT) {
        (void)snprintf(sunrise_buffer, sizeof(sunrise_buffer), "%s", "No rise");
    }
    else {
        screen_face_format_solar_time(&s_face_state.solar_events.sunrise,
                                      time_snapshot.is_24_hour,
                                      sunrise_buffer,
                                      sizeof(sunrise_buffer));
    }

    if(s_face_state.label_sunrise_value != NULL) {
        lv_label_set_text(s_face_state.label_sunrise_value, sunrise_buffer);
    }

    if(s_face_state.label_weather_low_value != NULL) {
        if(has_weather_data) {
            lv_label_set_text_fmt(s_face_state.label_weather_low_value,
                                  "L %dC",
                                  (int)weather_snapshot.low_temperature_c);
        }
        else {
            lv_label_set_text(s_face_state.label_weather_low_value, "L --");
        }
    }

    if(s_face_state.label_weather_high_value != NULL) {
        if(has_weather_data) {
            lv_label_set_text_fmt(s_face_state.label_weather_high_value,
                                  "H %dC",
                                  (int)weather_snapshot.high_temperature_c);
        }
        else {
            lv_label_set_text(s_face_state.label_weather_high_value, "H --");
        }
    }

    if(s_face_state.label_weather_average_value != NULL) {
        if(has_weather_data) {
            lv_label_set_text_fmt(s_face_state.label_weather_average_value,
                                  "%dC avg",
                                  (int)weather_snapshot.average_temperature_c);
        }
        else {
            lv_label_set_text(s_face_state.label_weather_average_value, "-- avg");
        }
    }

    screen_face_apply_visuals(phase, current_total_minutes, &s_face_state.solar_events, &time_snapshot);
}

static esp_err_t screen_face_build_layout(void)
{
    s_face_state.root = lv_obj_create(NULL);
    if(s_face_state.root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_remove_style_all(s_face_state.root);
    lv_obj_set_size(s_face_state.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_face_state.root, UI_COLOR_SOLAR_SKY_CENTER, 0);
    lv_obj_set_style_bg_opa(s_face_state.root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(s_face_state.root, UI_COLOR_SOLAR_SKY_EDGE, 0);
    lv_obj_set_style_bg_grad_dir(s_face_state.root, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(s_face_state.root, 0, 0);
    lv_obj_set_style_pad_all(s_face_state.root, 0, 0);

    s_face_state.time_ring = screen_face_create_ring_arc(s_face_state.root,
                                                         UI_FACE_TIME_RING_SIZE_PX,
                                                         UI_FACE_TIME_RING_WIDTH_PX);
    s_face_state.tectonic_ring = screen_face_create_ring_arc(s_face_state.root,
                                                             UI_FACE_INNER_RING_SIZE_PX,
                                                             UI_FACE_INNER_RING_WIDTH_PX);
    s_face_state.solar_ring = screen_face_create_ring_arc(s_face_state.root,
                                                          UI_FACE_OUTER_RING_SIZE_PX,
                                                          UI_FACE_OUTER_RING_WIDTH_PX);
    if(s_face_state.time_ring == NULL || s_face_state.tectonic_ring == NULL || s_face_state.solar_ring == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_align(s_face_state.time_ring, LV_ALIGN_TOP_MID, 0, UI_FACE_TIME_RING_Y_PX);
    lv_obj_align(s_face_state.tectonic_ring, LV_ALIGN_TOP_MID, 0, UI_FACE_INNER_RING_Y_PX);
    lv_obj_align(s_face_state.solar_ring, LV_ALIGN_TOP_MID, 0, UI_FACE_OUTER_RING_Y_PX);

    lv_arc_set_bg_angles(s_face_state.time_ring, 0, 359);
    lv_obj_set_style_arc_color(s_face_state.time_ring, UI_COLOR_RING_TIME, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_face_state.time_ring, UI_COLOR_PRIMARY_TEXT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_face_state.time_ring, UI_RING_TIME_OPA, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_face_state.time_ring, UI_RING_TIME_OPA, LV_PART_INDICATOR);
    lv_arc_set_value(s_face_state.time_ring, 0);

    lv_arc_set_bg_angles(s_face_state.tectonic_ring, 0, 359);
    lv_obj_set_style_arc_color(s_face_state.tectonic_ring, UI_COLOR_RING_TECTONIC, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_face_state.tectonic_ring, UI_COLOR_RING_TECTONIC, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_face_state.tectonic_ring, UI_RING_TECTONIC_OPA, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_face_state.tectonic_ring, UI_RING_TECTONIC_OPA, LV_PART_INDICATOR);
    lv_arc_set_value(s_face_state.tectonic_ring, 0);

    lv_arc_set_bg_angles(s_face_state.solar_ring, 0, 359);
    lv_obj_set_style_arc_color(s_face_state.solar_ring, UI_COLOR_SOLAR_NIGHT_ARC, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_face_state.solar_ring, UI_COLOR_RING_SOLAR, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_face_state.solar_ring, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_face_state.solar_ring, UI_RING_SOLAR_OPA, LV_PART_INDICATOR);
    lv_arc_set_value(s_face_state.solar_ring, 100);

    ESP_RETURN_ON_ERROR(screen_face_create_cardinal_markers(), "screen_face", "failed to create cardinal markers");

    s_face_state.solar_orb = lv_obj_create(s_face_state.root);
    if(s_face_state.solar_orb == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_remove_style_all(s_face_state.solar_orb);
    lv_obj_set_size(s_face_state.solar_orb, UI_FACE_SOLAR_MARKER_SIZE_PX, UI_FACE_SOLAR_MARKER_SIZE_PX);
    lv_obj_set_style_radius(s_face_state.solar_orb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_face_state.solar_orb, UI_COLOR_SOLAR_SUN_CORE, 0);
    lv_obj_set_style_bg_opa(s_face_state.solar_orb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_face_state.solar_orb, 0, 0);
    lv_obj_set_style_shadow_width(s_face_state.solar_orb, UI_SOLAR_DIAL_SUN_SHADOW_WIDTH_PX, 0);
    lv_obj_set_style_shadow_spread(s_face_state.solar_orb, 0, 0);
    lv_obj_set_style_shadow_color(s_face_state.solar_orb, UI_COLOR_SOLAR_SUN_CORE, 0);
    lv_obj_set_style_shadow_opa(s_face_state.solar_orb, UI_SOLAR_DIAL_SUN_SHADOW_OPA, 0);

    s_face_state.label_time = lv_label_create(s_face_state.root);
    if(s_face_state.label_time == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(s_face_state.label_time, UI_FONT_SOLAR_TIME, 0);
    lv_obj_set_style_text_color(s_face_state.label_time, UI_COLOR_PRIMARY_TEXT, 0);
    lv_obj_align(s_face_state.label_time, LV_ALIGN_TOP_MID, 0, UI_FACE_TIME_Y_PX);

    s_face_state.label_date = lv_label_create(s_face_state.root);
    if(s_face_state.label_date == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(s_face_state.label_date, UI_FONT_SOLAR_DATE, 0);
    lv_obj_set_style_text_color(s_face_state.label_date, UI_COLOR_SOLAR_COMPLICATION, 0);
    lv_obj_align(s_face_state.label_date, LV_ALIGN_TOP_RIGHT, -UI_FACE_DATE_RIGHT_OFFSET_PX, UI_FACE_DATE_Y_PX);

    s_face_state.label_day = lv_label_create(s_face_state.root);
    if(s_face_state.label_day == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(s_face_state.label_day, UI_FONT_SOLAR_COMPLICATION, 0);
    lv_obj_set_style_text_color(s_face_state.label_day, UI_COLOR_PRIMARY_TEXT, 0);
    lv_obj_align(s_face_state.label_day, LV_ALIGN_TOP_RIGHT, -UI_FACE_DATE_RIGHT_OFFSET_PX, UI_FACE_DAY_TOP_OFFSET_PX);

    s_face_state.label_ring_legend = lv_label_create(s_face_state.root);
    if(s_face_state.label_ring_legend == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(s_face_state.label_ring_legend, UI_FONT_SOLAR_COMPLICATION, 0);
    lv_obj_set_style_text_color(s_face_state.label_ring_legend, UI_COLOR_SECONDARY_TEXT, 0);
    lv_obj_set_style_text_letter_space(s_face_state.label_ring_legend, 2, 0);
    lv_label_set_text(s_face_state.label_ring_legend, "TIME  PLATE  SUN");
    lv_obj_align(s_face_state.label_ring_legend, LV_ALIGN_TOP_MID, 0, 286);

    s_face_state.complication_sunrise = screen_face_create_complication(s_face_state.root,
                                                                        LV_ALIGN_TOP_LEFT,
                                                                        UI_FACE_COMPLICATION_OFFSET_X_PX,
                                                                        UI_FACE_COMPLICATION_TOP_Y_PX,
                                                                        false,
                                                                        UI_COLOR_RING_SOLAR,
                                                                        &s_face_state.label_sunrise_value);
    s_face_state.complication_weather_low = screen_face_create_complication(s_face_state.root,
                                                                            LV_ALIGN_BOTTOM_LEFT,
                                                                            UI_FACE_COMPLICATION_OFFSET_X_PX,
                                                                            -UI_FACE_COMPLICATION_BOTTOM_Y_PX,
                                                                            false,
                                                                            UI_COLOR_ACCENT_BLUE,
                                                                            &s_face_state.label_weather_low_value);
    s_face_state.complication_weather_high = screen_face_create_complication(s_face_state.root,
                                                                             LV_ALIGN_BOTTOM_RIGHT,
                                                                             -UI_FACE_COMPLICATION_OFFSET_X_PX,
                                                                             -UI_FACE_COMPLICATION_BOTTOM_Y_PX,
                                                                             true,
                                                                             UI_COLOR_ACCENT_RED,
                                                                             &s_face_state.label_weather_high_value);
     if(s_face_state.complication_sunrise == NULL ||
         s_face_state.complication_weather_low == NULL ||
       s_face_state.complication_weather_high == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_face_state.label_weather_average_value = lv_label_create(s_face_state.root);
    if(s_face_state.label_weather_average_value == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(s_face_state.label_weather_average_value, UI_FONT_SOLAR_COMPLICATION, 0);
    lv_obj_set_style_text_color(s_face_state.label_weather_average_value, UI_COLOR_PRIMARY_TEXT, 0);
    lv_obj_align(s_face_state.label_weather_average_value, LV_ALIGN_TOP_MID, 0, UI_FACE_BOTTOM_CENTER_Y_PX);

    s_face_state.clock_timer = lv_timer_create(screen_face_clock_timer_cb, UI_CLOCK_UPDATE_PERIOD_MS, NULL);
    if(s_face_state.clock_timer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    screen_face_refresh();
    s_face_state.initialized = true;
    return ESP_OK;
}

static esp_err_t screen_face_refresh_day_events(const time_utils_snapshot_t *time_snapshot)
{
    solar_calc_date_t local_date;
    solar_calc_location_t location;
    solar_calc_status_t status;

    if(time_snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(s_face_state.cached_day_of_year == (time_snapshot->local_time.tm_yday + 1) &&
       s_face_state.cached_utc_offset_minutes == time_snapshot->utc_offset_minutes) {
        return ESP_OK;
    }

    local_date.year = time_snapshot->local_time.tm_year + 1900;
    local_date.month = time_snapshot->local_time.tm_mon + 1;
    local_date.day = time_snapshot->local_time.tm_mday;

    location.latitude_deg = WATCH_OS_DEFAULT_LATITUDE;
    location.longitude_deg = WATCH_OS_DEFAULT_LONGITUDE;
    location.utc_offset_minutes = time_snapshot->utc_offset_minutes;

    status = solar_calc_calculate_sun_times(&location, &local_date, &s_face_state.solar_events);
    if(status == SOLAR_CALC_STATUS_INVALID_ARG) {
        return ESP_ERR_INVALID_STATE;
    }

    s_face_state.cached_day_of_year = time_snapshot->local_time.tm_yday + 1;
    s_face_state.cached_utc_offset_minutes = time_snapshot->utc_offset_minutes;
    return ESP_OK;
}

static screen_face_phase_t screen_face_get_phase(uint16_t current_total_minutes,
                                                 const solar_calc_day_events_t *solar_events)
{
    uint16_t minutes_from_sunrise;

    if(solar_events == NULL) {
        return SCREEN_FACE_PHASE_NIGHT;
    }

    if(solar_events->status == SOLAR_CALC_STATUS_POLAR_DAY) {
        return SCREEN_FACE_PHASE_DAY;
    }

    if(solar_events->status == SOLAR_CALC_STATUS_POLAR_NIGHT || solar_events->daylight_minutes == 0U) {
        return SCREEN_FACE_PHASE_NIGHT;
    }

    if(current_total_minutes < solar_events->sunrise.total_minutes ||
       current_total_minutes > solar_events->sunset.total_minutes) {
        return SCREEN_FACE_PHASE_NIGHT;
    }

    minutes_from_sunrise = (uint16_t)(current_total_minutes - solar_events->sunrise.total_minutes);
    if(minutes_from_sunrise < (solar_events->daylight_minutes / 4U)) {
        return SCREEN_FACE_PHASE_MORNING;
    }

    if(minutes_from_sunrise > ((solar_events->daylight_minutes * 3U) / 4U)) {
        return SCREEN_FACE_PHASE_EVENING;
    }

    return SCREEN_FACE_PHASE_DAY;
}

static void screen_face_apply_visuals(screen_face_phase_t phase,
                                      uint16_t current_total_minutes,
                                      const solar_calc_day_events_t *solar_events,
                                      const time_utils_snapshot_t *time_snapshot)
{
    lv_color_t arc_color = UI_COLOR_SOLAR_DAY_ARC;
    lv_color_t orb_color = UI_COLOR_SOLAR_SUN_CORE;
    lv_opa_t solar_ring_opa = UI_RING_SOLAR_OPA;
    uint16_t time_ring_value;
    uint16_t tectonic_ring_value;
    float sunrise_angle_deg = 0.0f;
    float current_angle_deg;

    if(solar_events == NULL) {
        return;
    }

    current_angle_deg = screen_face_clock_minutes_to_ring_angle_deg(current_total_minutes);

    switch(phase) {
        case SCREEN_FACE_PHASE_MORNING:
            arc_color = UI_COLOR_SOLAR_DAY_ARC;
            break;
        case SCREEN_FACE_PHASE_DAY:
            arc_color = UI_COLOR_SOLAR_DAY_ARC;
            break;
        case SCREEN_FACE_PHASE_EVENING:
            arc_color = UI_COLOR_SOLAR_DAY_ARC;
            break;
        case SCREEN_FACE_PHASE_NIGHT:
        default:
            arc_color = UI_COLOR_SOLAR_NIGHT_ARC;
            orb_color = UI_COLOR_SOLAR_COMPLICATION;
            break;
    }

    if(solar_events->status == SOLAR_CALC_STATUS_POLAR_DAY) {
        sunrise_angle_deg = 0.0f;
    }
    else if(solar_events->status == SOLAR_CALC_STATUS_OK) {
        sunrise_angle_deg = screen_face_clock_minutes_to_ring_angle_deg(solar_events->sunrise.total_minutes);
        if(current_total_minutes < solar_events->sunrise.total_minutes ||
           current_total_minutes > solar_events->sunset.total_minutes) {
            solar_ring_opa = LV_OPA_20;
        }
    }
    else {
        solar_ring_opa = LV_OPA_0;
    }

    time_ring_value = (uint16_t)((current_total_minutes * 100U) / SCREEN_FACE_MINUTES_PER_DAY);
    tectonic_ring_value = (uint16_t)(((uint16_t)screen_face_get_demo_plate_angle_deg(time_snapshot) * 100U) / 180U);

    lv_obj_set_style_bg_color(s_face_state.root, UI_COLOR_SOLAR_SKY_CENTER, 0);
    lv_obj_set_style_bg_grad_color(s_face_state.root, UI_COLOR_SOLAR_SKY_EDGE, 0);

    if(s_face_state.time_ring != NULL) {
        lv_arc_set_value(s_face_state.time_ring, time_ring_value);
    }

    if(s_face_state.tectonic_ring != NULL) {
        lv_arc_set_value(s_face_state.tectonic_ring, tectonic_ring_value);
    }

    if(s_face_state.solar_ring != NULL) {
        if(solar_events->status == SOLAR_CALC_STATUS_POLAR_DAY) {
            lv_arc_set_bg_angles(s_face_state.solar_ring, 0, 359);
            lv_obj_set_style_arc_opa(s_face_state.solar_ring, LV_OPA_80, LV_PART_INDICATOR);
        }
        else if(solar_events->status == SOLAR_CALC_STATUS_OK &&
                current_total_minutes > solar_events->sunrise.total_minutes &&
                current_total_minutes <= solar_events->sunset.total_minutes) {
            lv_arc_set_bg_angles(s_face_state.solar_ring,
                                 screen_face_angle_to_lv_deg(sunrise_angle_deg),
                                 screen_face_angle_to_lv_deg(current_angle_deg));
            lv_obj_set_style_arc_opa(s_face_state.solar_ring, LV_OPA_COVER, LV_PART_INDICATOR);
        }
        else {
            lv_arc_set_bg_angles(s_face_state.solar_ring,
                                 screen_face_angle_to_lv_deg(sunrise_angle_deg),
                                 screen_face_angle_to_lv_deg(sunrise_angle_deg + 1.0f));
            lv_obj_set_style_arc_opa(s_face_state.solar_ring, solar_ring_opa, LV_PART_INDICATOR);
        }

        lv_obj_set_style_arc_color(s_face_state.solar_ring, arc_color, LV_PART_INDICATOR);
        lv_arc_set_value(s_face_state.solar_ring, 100);
    }

    if(s_face_state.solar_orb != NULL) {
        lv_obj_set_style_bg_color(s_face_state.solar_orb, orb_color, 0);
        lv_obj_set_style_shadow_color(s_face_state.solar_orb, orb_color, 0);
        lv_obj_set_style_shadow_opa(s_face_state.solar_orb,
                                    (phase == SCREEN_FACE_PHASE_NIGHT) ? LV_OPA_20 : UI_SOLAR_DIAL_SUN_SHADOW_OPA,
                                    0);
        screen_face_position_on_dial(s_face_state.solar_orb,
                                     UI_FACE_SOLAR_MARKER_SIZE_PX,
                                     current_angle_deg,
                                     UI_FACE_SOLAR_RADIUS_PX);
    }
}

static lv_obj_t *screen_face_create_ring_arc(lv_obj_t *parent, lv_coord_t size, lv_coord_t width)
{
    lv_obj_t *arc = lv_arc_create(parent);

    if(arc == NULL) {
        return NULL;
    }

    lv_obj_set_size(arc, size, size);
    lv_arc_set_rotation(arc, 0);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 100);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    return arc;
}

static esp_err_t screen_face_create_cardinal_markers(void)
{
    static const float marker_angles_deg[UI_SOLAR_DIAL_CARDINAL_MARKER_COUNT] = {0.0f, 90.0f, 180.0f, 270.0f};
    uint32_t marker_index;

    for(marker_index = 0; marker_index < UI_SOLAR_DIAL_CARDINAL_MARKER_COUNT; ++marker_index) {
        const bool vertical = ((marker_index % 2U) == 0U);
        const lv_coord_t width = vertical ? UI_SOLAR_DIAL_CARDINAL_MARKER_SHORT_PX : UI_SOLAR_DIAL_CARDINAL_MARKER_LONG_PX;
        const lv_coord_t height = vertical ? UI_SOLAR_DIAL_CARDINAL_MARKER_LONG_PX : UI_SOLAR_DIAL_CARDINAL_MARKER_SHORT_PX;

        s_face_state.hour_markers[marker_index] = lv_obj_create(s_face_state.root);
        if(s_face_state.hour_markers[marker_index] == NULL) {
            return ESP_ERR_NO_MEM;
        }

        lv_obj_remove_style_all(s_face_state.hour_markers[marker_index]);
        lv_obj_set_size(s_face_state.hour_markers[marker_index], width, height);
        lv_obj_set_style_radius(s_face_state.hour_markers[marker_index], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_face_state.hour_markers[marker_index], UI_COLOR_PRIMARY_TEXT, 0);
        lv_obj_set_style_bg_opa(s_face_state.hour_markers[marker_index], UI_SOLAR_DIAL_CARDINAL_MARKER_OPA, 0);
        screen_face_position_on_dial(s_face_state.hour_markers[marker_index],
                                     vertical ? height : width,
                                     marker_angles_deg[marker_index],
                                     UI_SOLAR_DIAL_CARDINAL_MARKER_RADIUS_PX);
    }

    return ESP_OK;
}

static lv_obj_t *screen_face_create_complication(lv_obj_t *parent,
                                                 lv_align_t align,
                                                 lv_coord_t x_offset,
                                                 lv_coord_t y_offset,
                                                 bool right_aligned,
                                                 lv_color_t icon_color,
                                                 lv_obj_t **value_label)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_t *icon;

    if(container == NULL) {
        return NULL;
    }

    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, UI_FACE_COMPLICATION_WIDTH_PX, UI_FACE_COMPLICATION_HEIGHT_PX);
    lv_obj_align(container, align, x_offset, y_offset);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(container, right_aligned ? LV_FLEX_FLOW_ROW_REVERSE : LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container,
                          right_aligned ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_pad_gap(container, 6, 0);

    icon = lv_obj_create(container);
    if(icon == NULL) {
        return NULL;
    }

    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, UI_FACE_COMPLICATION_ICON_SIZE_PX, UI_FACE_COMPLICATION_ICON_SIZE_PX);
    lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(icon, icon_color, 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);

    if(value_label != NULL) {
        *value_label = lv_label_create(container);
        if(*value_label == NULL) {
            return NULL;
        }

        lv_obj_set_style_text_font(*value_label, UI_FONT_SOLAR_COMPLICATION, 0);
        lv_obj_set_style_text_color(*value_label, UI_COLOR_PRIMARY_TEXT, 0);
    }

    return container;
}

static void screen_face_position_on_dial(lv_obj_t *object, lv_coord_t size_px, float dial_angle_deg, lv_coord_t radius_px)
{
    const lv_coord_t lv_angle_deg = (lv_coord_t)dial_angle_deg - 90;
    const lv_coord_t center_x = UI_SOLAR_DIAL_CENTER_X_PX;
    const lv_coord_t center_y = UI_SOLAR_DIAL_CENTER_Y_PX;
    const lv_coord_t pos_x = (lv_coord_t)(center_x + ((lv_trigo_cos(lv_angle_deg) * radius_px) >> LV_TRIGO_SHIFT));
    const lv_coord_t pos_y = (lv_coord_t)(center_y + ((lv_trigo_sin(lv_angle_deg) * radius_px) >> LV_TRIGO_SHIFT));

    if(object == NULL) {
        return;
    }

    lv_obj_set_pos(object, pos_x - (size_px / 2), pos_y - (size_px / 2));
}

static float screen_face_clock_minutes_to_ring_angle_deg(uint16_t total_minutes)
{
    return ((float)(total_minutes % SCREEN_FACE_MINUTES_PER_DAY) * 360.0f) / (float)SCREEN_FACE_MINUTES_PER_DAY;
}

static lv_coord_t screen_face_angle_to_lv_deg(float angle_deg)
{
    int32_t wrapped_angle = (int32_t)angle_deg;

    while(wrapped_angle < 0) {
        wrapped_angle += 360;
    }

    while(wrapped_angle >= 360) {
        wrapped_angle -= 360;
    }

    wrapped_angle -= 90;
    if(wrapped_angle < 0) {
        wrapped_angle += 360;
    }

    return (lv_coord_t)wrapped_angle;
}

static void screen_face_format_solar_time(const solar_calc_clock_time_t *clock_time,
                                          bool is_24_hour,
                                          char *buffer,
                                          size_t buffer_size)
{
    uint8_t display_hour;

    if(clock_time == NULL || buffer == NULL || buffer_size == 0U) {
        return;
    }

    if(is_24_hour) {
        (void)snprintf(buffer, buffer_size, "%u:%02u", (unsigned int)clock_time->hour, (unsigned int)clock_time->minute);
        return;
    }

    display_hour = clock_time->hour % 12U;
    if(display_hour == 0U) {
        display_hour = 12U;
    }

    (void)snprintf(buffer, buffer_size, "%u:%02u", (unsigned int)display_hour, (unsigned int)clock_time->minute);
}

static uint8_t screen_face_get_demo_plate_angle_deg(const time_utils_snapshot_t *time_snapshot)
{
    if(time_snapshot == NULL) {
        return 48U;
    }

    return (uint8_t)(35U + (uint8_t)(((time_snapshot->local_time.tm_yday * 3) + time_snapshot->local_time.tm_hour) % 110U));
}

static void screen_face_clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    screen_face_refresh();
}