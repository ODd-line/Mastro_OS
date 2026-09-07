/**
 * @file screen_settings.c
 * @brief System settings screen implementation.
 */

#include "ui/screens/screen_settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "apps/app_registry.h"
#include "config.h"
#include "esp_err.h"
#include "hal/hal_display.h"
#include "ui/ui_manager.h"
#include "ui/ui_defs.h"
#include "utils/time_utils.h"
#include "utils/weather_service.h"
#include "utils/wifi_manager.h"

typedef struct {
    bool initialized;
    bool raise_to_wake_enabled;
    bool haptics_enabled;
    uint8_t brightness_percent;
    lv_obj_t *root;
    lv_obj_t *content;
    lv_obj_t *value_brightness;
    lv_obj_t *value_apps;
    lv_obj_t *value_time_format;
    lv_obj_t *value_timezone;
    lv_obj_t *value_location;
    lv_obj_t *value_wifi;
    lv_obj_t *value_weather;
    lv_obj_t *wifi_ssid_input;
    lv_obj_t *wifi_password_input;
    lv_obj_t *wifi_action_status;
    lv_obj_t *wifi_keyboard;
    lv_obj_t *switch_raise_to_wake;
    lv_obj_t *switch_haptics;
    lv_obj_t *slider_brightness;
} screen_settings_state_t;

static screen_settings_state_t s_settings_state;

static esp_err_t screen_settings_build_layout(void);
static lv_obj_t *screen_settings_create_row(lv_obj_t *parent, const char *title, lv_obj_t **value_label);
static lv_obj_t *screen_settings_create_action_button(lv_obj_t *parent, const char *label, lv_event_cb_t event_cb);
static void screen_settings_update_action_status(const char *message);
static void screen_settings_switch_event_cb(lv_event_t *event);
static void screen_settings_slider_event_cb(lv_event_t *event);
static void screen_settings_wifi_textarea_event_cb(lv_event_t *event);
static void screen_settings_wifi_keyboard_event_cb(lv_event_t *event);
static void screen_settings_save_wifi_event_cb(lv_event_t *event);
static void screen_settings_reconnect_wifi_event_cb(lv_event_t *event);
static void screen_settings_refresh_weather_event_cb(lv_event_t *event);

/** {@inheritDoc screen_settings_init} */
esp_err_t screen_settings_init(void)
{
    if(s_settings_state.initialized) {
        screen_settings_refresh();
        return ESP_OK;
    }

    memset(&s_settings_state, 0, sizeof(s_settings_state));
    s_settings_state.raise_to_wake_enabled = true;
    s_settings_state.haptics_enabled = true;
    s_settings_state.brightness_percent = hal_display_get_brightness();
    return screen_settings_build_layout();
}

/** {@inheritDoc screen_settings_get_root} */
lv_obj_t *screen_settings_get_root(void)
{
    return s_settings_state.root;
}

/** {@inheritDoc screen_settings_refresh} */
void screen_settings_refresh(void)
{
    char app_count_buffer[UI_SETTING_VALUE_WIDTH_PX / 2];
    char location_buffer[24];
    char timezone_buffer[WATCH_OS_TIMEZONE_MAX_LENGTH];
    char wifi_ssid_buffer[33];
    char wifi_value_buffer[48];
    char weather_value_buffer[48];
    weather_service_snapshot_t weather_snapshot;

    if(s_settings_state.value_apps == NULL || s_settings_state.value_brightness == NULL) {
        return;
    }

    s_settings_state.brightness_percent = hal_display_get_brightness();
    lv_label_set_text_fmt(s_settings_state.value_brightness, "%u%%", s_settings_state.brightness_percent);
    snprintf(app_count_buffer, sizeof(app_count_buffer), "%u", (unsigned int)app_registry_get_total_count());
    lv_label_set_text(s_settings_state.value_apps, app_count_buffer);

    if(s_settings_state.slider_brightness != NULL) {
        lv_slider_set_value(s_settings_state.slider_brightness, s_settings_state.brightness_percent, LV_ANIM_OFF);
    }

    if(s_settings_state.value_time_format != NULL) {
        lv_label_set_text(s_settings_state.value_time_format,
                          time_utils_is_24_hour_enabled() ? "24-hour" : "12-hour");
    }

    if(s_settings_state.value_timezone != NULL &&
       time_utils_get_timezone(timezone_buffer, sizeof(timezone_buffer)) == ESP_OK) {
        lv_label_set_text(s_settings_state.value_timezone, timezone_buffer);
    }

    if(s_settings_state.value_location != NULL) {
        snprintf(location_buffer,
                 sizeof(location_buffer),
                 "%.2f, %.2f",
                 (double)WATCH_OS_DEFAULT_LATITUDE,
                 (double)WATCH_OS_DEFAULT_LONGITUDE);
        lv_label_set_text(s_settings_state.value_location, location_buffer);
    }

    if(s_settings_state.value_wifi != NULL) {
        const wifi_manager_state_t wifi_state = wifi_manager_get_state();
        const bool has_ssid = (wifi_manager_get_ssid(wifi_ssid_buffer, sizeof(wifi_ssid_buffer)) == ESP_OK) &&
                              (wifi_ssid_buffer[0] != '\0');
        if(wifi_state == WIFI_MANAGER_STATE_CONNECTED && has_ssid) {
            snprintf(wifi_value_buffer, sizeof(wifi_value_buffer), "%s", wifi_ssid_buffer);
        }
        else if(wifi_state == WIFI_MANAGER_STATE_CONNECTING) {
            snprintf(wifi_value_buffer, sizeof(wifi_value_buffer), "%s", "Connecting");
        }
        else if(wifi_manager_is_configured() && has_ssid) {
            snprintf(wifi_value_buffer, sizeof(wifi_value_buffer), "%s", wifi_ssid_buffer);
        }
        else {
            snprintf(wifi_value_buffer, sizeof(wifi_value_buffer), "%s", "Not set");
        }
        lv_label_set_text(s_settings_state.value_wifi, wifi_value_buffer);

        if(s_settings_state.wifi_ssid_input != NULL &&
           has_ssid &&
           !lv_obj_has_state(s_settings_state.wifi_ssid_input, LV_STATE_FOCUSED) &&
           strcmp(lv_textarea_get_text(s_settings_state.wifi_ssid_input), wifi_ssid_buffer) != 0) {
            lv_textarea_set_text(s_settings_state.wifi_ssid_input, wifi_ssid_buffer);
        }
    }

    if(s_settings_state.value_weather != NULL) {
        if(weather_service_get_snapshot(&weather_snapshot) == ESP_OK) {
            if(weather_snapshot.sync_in_progress) {
                lv_label_set_text(s_settings_state.value_weather, "Syncing");
            }
            else if(weather_snapshot.has_data) {
                snprintf(weather_value_buffer,
                         sizeof(weather_value_buffer),
                         "%d/%dC",
                         (int)weather_snapshot.low_temperature_c,
                         (int)weather_snapshot.high_temperature_c);
                lv_label_set_text(s_settings_state.value_weather, weather_value_buffer);
            }
            else if(wifi_manager_is_connected()) {
                lv_label_set_text(s_settings_state.value_weather, "Waiting");
            }
            else {
                lv_label_set_text(s_settings_state.value_weather, "Offline");
            }
        }
    }
}

static esp_err_t screen_settings_build_layout(void)
{
    lv_obj_t *header_label;
    lv_obj_t *row;
    lv_obj_t *title_label;

    s_settings_state.root = lv_obj_create(NULL);
    if(s_settings_state.root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_remove_style_all(s_settings_state.root);
    lv_obj_set_size(s_settings_state.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_settings_state.root, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(s_settings_state.root, LV_OPA_COVER, 0);

    header_label = lv_label_create(s_settings_state.root);
    if(header_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_style_text_font(header_label, UI_FONT_TIME, 0);
    lv_obj_set_style_text_color(header_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_label_set_text(header_label, "Settings");
    lv_obj_align(header_label, LV_ALIGN_TOP_LEFT, UI_HEADER_SIDE_PADDING_PX, UI_HEADER_TOP_OFFSET_PX);

    s_settings_state.content = lv_obj_create(s_settings_state.root);
    if(s_settings_state.content == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_size(s_settings_state.content,
                    UI_SCREEN_WIDTH - (2 * UI_HEADER_SIDE_PADDING_PX),
                    UI_SCREEN_HEIGHT - (UI_HEADER_TOP_OFFSET_PX + 64));
    lv_obj_align(s_settings_state.content, LV_ALIGN_BOTTOM_MID, 0, -UI_EDGE_PADDING_PX);
    lv_obj_set_style_bg_opa(s_settings_state.content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_settings_state.content, 0, 0);
    lv_obj_set_style_pad_all(s_settings_state.content, 0, 0);
    lv_obj_set_style_pad_gap(s_settings_state.content, 8, 0);
    lv_obj_set_flex_flow(s_settings_state.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_settings_state.content, LV_DIR_VER);

    row = screen_settings_create_row(s_settings_state.content, "Brightness", &s_settings_state.value_brightness);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_settings_state.slider_brightness = lv_slider_create(row);
    if(s_settings_state.slider_brightness == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_width(s_settings_state.slider_brightness, UI_SLIDER_WIDTH_PX);
    lv_slider_set_range(s_settings_state.slider_brightness, 10, 100);
    lv_slider_set_value(s_settings_state.slider_brightness, s_settings_state.brightness_percent, LV_ANIM_OFF);
    lv_obj_align(s_settings_state.slider_brightness, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(s_settings_state.slider_brightness, screen_settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    row = screen_settings_create_row(s_settings_state.content, "Raise to Wake", NULL);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_settings_state.switch_raise_to_wake = lv_switch_create(row);
    if(s_settings_state.switch_raise_to_wake == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_align(s_settings_state.switch_raise_to_wake, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_state(s_settings_state.switch_raise_to_wake, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_settings_state.switch_raise_to_wake, screen_settings_switch_event_cb, LV_EVENT_VALUE_CHANGED, &s_settings_state.raise_to_wake_enabled);

    row = screen_settings_create_row(s_settings_state.content, "Haptics", NULL);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_settings_state.switch_haptics = lv_switch_create(row);
    if(s_settings_state.switch_haptics == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_align(s_settings_state.switch_haptics, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_state(s_settings_state.switch_haptics, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_settings_state.switch_haptics, screen_settings_switch_event_cb, LV_EVENT_VALUE_CHANGED, &s_settings_state.haptics_enabled);

    row = screen_settings_create_row(s_settings_state.content, "Time Format", &s_settings_state.value_time_format);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    title_label = lv_label_create(row);
    if(title_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_label_set_text(title_label, "Tap the watch face time to switch");
    lv_obj_align(title_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    row = screen_settings_create_row(s_settings_state.content, "Timezone", &s_settings_state.value_timezone);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    title_label = lv_label_create(row);
    if(title_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_label_set_text(title_label, "Used for local time and sunrise");
    lv_obj_align(title_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    row = screen_settings_create_row(s_settings_state.content, "Location", &s_settings_state.value_location);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    title_label = lv_label_create(row);
    if(title_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_label_set_text(title_label, "Used for solar and weather data");
    lv_obj_align(title_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    row = screen_settings_create_row(s_settings_state.content, "Wi-Fi", &s_settings_state.value_wifi);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    title_label = lv_label_create(row);
    if(title_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_label_set_text(title_label, "Reconnects automatically when available");
    lv_obj_align(title_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    row = screen_settings_create_row(s_settings_state.content, "Weather", &s_settings_state.value_weather);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    title_label = lv_label_create(row);
    if(title_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_label_set_text(title_label, "Updates automatically while connected");
    lv_obj_align(title_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    row = lv_obj_create(s_settings_state.content);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 196);
    lv_obj_set_style_bg_color(row, UI_COLOR_CONTROL_TILE, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, UI_CORNER_RADIUS_PX, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, UI_EDGE_PADDING_PX, 0);
    lv_obj_set_style_pad_gap(row, UI_SMALL_GAP_PX, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    title_label = lv_label_create(row);
    if(title_label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_label_set_text(title_label, "Wi-Fi Setup");

    s_settings_state.wifi_ssid_input = lv_textarea_create(row);
    if(s_settings_state.wifi_ssid_input == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_width(s_settings_state.wifi_ssid_input, lv_pct(100));
    lv_textarea_set_one_line(s_settings_state.wifi_ssid_input, true);
    lv_textarea_set_placeholder_text(s_settings_state.wifi_ssid_input, "SSID");
    lv_obj_add_event_cb(s_settings_state.wifi_ssid_input,
                        screen_settings_wifi_textarea_event_cb,
                        LV_EVENT_FOCUSED,
                        NULL);

    s_settings_state.wifi_password_input = lv_textarea_create(row);
    if(s_settings_state.wifi_password_input == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_width(s_settings_state.wifi_password_input, lv_pct(100));
    lv_textarea_set_one_line(s_settings_state.wifi_password_input, true);
    lv_textarea_set_password_mode(s_settings_state.wifi_password_input, true);
    lv_textarea_set_placeholder_text(s_settings_state.wifi_password_input, "Password");
    lv_obj_add_event_cb(s_settings_state.wifi_password_input,
                        screen_settings_wifi_textarea_event_cb,
                        LV_EVENT_FOCUSED,
                        NULL);

    title_label = lv_label_create(row);
    if(title_label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_label_set_text(title_label, "Saved on this watch");

    lv_obj_t *button_row = lv_obj_create(row);
    if(button_row == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_width(button_row, lv_pct(100));
    lv_obj_set_height(button_row, 44);
    lv_obj_set_style_bg_opa(button_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(button_row, 0, 0);
    lv_obj_set_style_pad_all(button_row, 0, 0);
    lv_obj_set_style_pad_gap(button_row, UI_SMALL_GAP_PX, 0);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_clear_flag(button_row, LV_OBJ_FLAG_SCROLLABLE);

    if(screen_settings_create_action_button(button_row, "Save", screen_settings_save_wifi_event_cb) == NULL ||
       screen_settings_create_action_button(button_row, "Reconnect", screen_settings_reconnect_wifi_event_cb) == NULL ||
       screen_settings_create_action_button(button_row, "Refresh Weather", screen_settings_refresh_weather_event_cb) == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_settings_state.wifi_action_status = lv_label_create(row);
    if(s_settings_state.wifi_action_status == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(s_settings_state.wifi_action_status, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(s_settings_state.wifi_action_status, UI_COLOR_SECONDARY_TEXT, 0);
    lv_label_set_text(s_settings_state.wifi_action_status, "Idle");

    s_settings_state.wifi_keyboard = lv_keyboard_create(s_settings_state.root);
    if(s_settings_state.wifi_keyboard == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_size(s_settings_state.wifi_keyboard, UI_SCREEN_WIDTH, 164);
    lv_obj_align(s_settings_state.wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_settings_state.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_settings_state.wifi_keyboard, screen_settings_wifi_keyboard_event_cb, LV_EVENT_ALL, NULL);

    row = screen_settings_create_row(s_settings_state.content, "Installed Apps", &s_settings_state.value_apps);
    if(row == NULL) {
        return ESP_ERR_NO_MEM;
    }

    title_label = lv_label_create(row);
    if(title_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_label_set_text(title_label, "Available in the app honeycomb");
    lv_obj_align(title_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    screen_settings_refresh();
    s_settings_state.initialized = true;
    return ESP_OK;
}

static lv_obj_t *screen_settings_create_row(lv_obj_t *parent, const char *title, lv_obj_t **value_label)
{
    lv_obj_t *row;
    lv_obj_t *title_label;

    row = lv_obj_create(parent);
    if(row == NULL) {
        return NULL;
    }

    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 72);
    lv_obj_set_style_bg_color(row, UI_COLOR_CONTROL_TILE, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 14, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 12, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    title_label = lv_label_create(row);
    if(title_label == NULL) {
        return NULL;
    }

    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    if(value_label != NULL) {
        *value_label = lv_label_create(row);
        if(*value_label == NULL) {
            return NULL;
        }

        lv_obj_set_style_text_font(*value_label, UI_FONT_LABEL, 0);
        lv_obj_set_style_text_color(*value_label, UI_COLOR_SECONDARY_TEXT, 0);
        lv_obj_align(*value_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    }

    return row;
}

static lv_obj_t *screen_settings_create_action_button(lv_obj_t *parent, const char *label, lv_event_cb_t event_cb)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *button_label;

    if(button == NULL) {
        return NULL;
    }

    lv_obj_set_height(button, 40);
    lv_obj_set_style_radius(button, UI_CORNER_RADIUS_PX, 0);
    lv_obj_set_style_bg_color(button, UI_COLOR_ACCENT_BLUE, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_hor(button, UI_EDGE_PADDING_PX, 0);
    lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, NULL);

    button_label = lv_label_create(button);
    if(button_label == NULL) {
        return NULL;
    }

    lv_obj_set_style_text_font(button_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(button_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_label_set_text(button_label, label);
    lv_obj_center(button_label);
    return button;
}

static void screen_settings_update_action_status(const char *message)
{
    if(s_settings_state.wifi_action_status != NULL && message != NULL) {
        lv_label_set_text(s_settings_state.wifi_action_status, message);
    }
}

static void screen_settings_switch_event_cb(lv_event_t *event)
{
    bool *backing_value = (bool *)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);

    if(backing_value == NULL || target == NULL) {
        return;
    }

    *backing_value = lv_obj_has_state(target, LV_STATE_CHECKED);
}

static void screen_settings_slider_event_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);

    if(target == NULL) {
        return;
    }

    s_settings_state.brightness_percent = (uint8_t)lv_slider_get_value(target);
    (void)hal_display_set_brightness(s_settings_state.brightness_percent);
    screen_settings_refresh();
}

static void screen_settings_wifi_textarea_event_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);

    if(target == NULL || s_settings_state.wifi_keyboard == NULL) {
        return;
    }

    lv_keyboard_set_textarea(s_settings_state.wifi_keyboard, target);
    lv_obj_clear_flag(s_settings_state.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void screen_settings_wifi_keyboard_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);

    if(s_settings_state.wifi_keyboard == NULL) {
        return;
    }

    if(code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
        lv_keyboard_set_textarea(s_settings_state.wifi_keyboard, NULL);
        lv_obj_add_flag(s_settings_state.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
        if(s_settings_state.wifi_ssid_input != NULL) {
            lv_obj_clear_state(s_settings_state.wifi_ssid_input, LV_STATE_FOCUSED);
        }
        if(s_settings_state.wifi_password_input != NULL) {
            lv_obj_clear_state(s_settings_state.wifi_password_input, LV_STATE_FOCUSED);
        }
    }
}

static void screen_settings_save_wifi_event_cb(lv_event_t *event)
{
    const char *ssid;
    const char *password;

    (void)event;

    if(s_settings_state.wifi_ssid_input == NULL || s_settings_state.wifi_password_input == NULL) {
        return;
    }

    ssid = lv_textarea_get_text(s_settings_state.wifi_ssid_input);
    password = lv_textarea_get_text(s_settings_state.wifi_password_input);
    if(wifi_manager_set_credentials(ssid, password) == ESP_OK) {
        lv_textarea_set_text(s_settings_state.wifi_password_input, "");
        screen_settings_update_action_status("Credentials saved");
    }
    else {
        screen_settings_update_action_status("Save failed");
    }

    screen_settings_refresh();
}

static void screen_settings_reconnect_wifi_event_cb(lv_event_t *event)
{
    (void)event;

    if(wifi_manager_reconnect() == ESP_OK) {
        screen_settings_update_action_status("Reconnect requested");
    }
    else {
        screen_settings_update_action_status("Reconnect unavailable");
    }

    screen_settings_refresh();
}

static void screen_settings_refresh_weather_event_cb(lv_event_t *event)
{
    (void)event;

    if(weather_service_request_refresh() == ESP_OK) {
        screen_settings_update_action_status("Weather refresh requested");
    }
    else {
        screen_settings_update_action_status("Refresh unavailable");
    }

    screen_settings_refresh();
}