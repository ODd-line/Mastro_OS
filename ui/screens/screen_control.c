/**
 * @file screen_control.c
 * @brief Control center implementation.
 */

#include "ui/screens/screen_control.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/hal_display.h"
#include "ui/ui_defs.h"
#include "ui/ui_manager.h"
#include "ui/ui_styles.h"
#include "utils/time_utils.h"
#include "utils/weather_service.h"
#include "utils/wifi_manager.h"

#define SCREEN_CONTROL_TILE_COUNT 6U

typedef struct {
    uint8_t action;
    const char *label;
    const char *icon_symbol;
    uint32_t active_color_hex;
    bool enabled;
} control_tile_definition_t;

typedef enum {
    CONTROL_ACTION_WIFI = 0,
    CONTROL_ACTION_TIME_FORMAT,
    CONTROL_ACTION_SYNC,
    CONTROL_ACTION_TORCH,
    CONTROL_ACTION_THEATER,
    CONTROL_ACTION_SETTINGS,
} control_action_t;

typedef struct {
    bool initialized;
    lv_obj_t *root;
    lv_obj_t *container;
    lv_obj_t *status_label;
    lv_obj_t *brightness_slider;
    lv_obj_t *brightness_value_label;
    lv_obj_t *torch_overlay;
    lv_obj_t *tiles[SCREEN_CONTROL_TILE_COUNT];
    uint8_t brightness_before_theater;
    uint8_t brightness_before_torch;
} screen_control_state_t;

static screen_control_state_t s_control_state;

static control_tile_definition_t CONTROL_TILE_DEFINITIONS[] = {
    { CONTROL_ACTION_WIFI, "Wi-Fi", LV_SYMBOL_WIFI, UI_COLOR_ACCENT_BLUE_HEX, false },
    { CONTROL_ACTION_TIME_FORMAT, "24-hour", LV_SYMBOL_LIST, UI_COLOR_ACCENT_GREEN_HEX, true },
    { CONTROL_ACTION_SYNC, "Sync", LV_SYMBOL_REFRESH, UI_COLOR_ACCENT_BLUE_HEX, false },
    { CONTROL_ACTION_TORCH, "Torch", LV_SYMBOL_CHARGE, UI_COLOR_ACCENT_BLUE_HEX, false },
    { CONTROL_ACTION_THEATER, "Theater", LV_SYMBOL_EYE_CLOSE, UI_COLOR_ACCENT_RED_HEX, false },
    { CONTROL_ACTION_SETTINGS, "Settings", LV_SYMBOL_SETTINGS, UI_COLOR_ACCENT_GREEN_HEX, false },
};

static esp_err_t screen_control_build_layout(void);
static lv_obj_t *screen_control_create_tile(lv_obj_t *parent,
                                            control_tile_definition_t *tile_definition);
static void screen_control_update_tile_visuals(lv_obj_t *tile, const control_tile_definition_t *tile_definition);
static void screen_control_tile_event_cb(lv_event_t *event);
static void screen_control_brightness_event_cb(lv_event_t *event);
static void screen_control_torch_overlay_event_cb(lv_event_t *event);
static void screen_control_wifi_reconnect_task(void *task_parameter);
static void screen_control_sync_task(void *task_parameter);
static control_tile_definition_t *screen_control_find_definition(const char *label);

/** {@inheritDoc screen_control_init} */
esp_err_t screen_control_init(void)
{
    if(s_control_state.initialized) {
        return ESP_OK;
    }

    memset(&s_control_state, 0, sizeof(s_control_state));
    return screen_control_build_layout();
}

/** {@inheritDoc screen_control_get_root} */
lv_obj_t *screen_control_get_root(void)
{
    return s_control_state.root;
}

void screen_control_refresh(void)
{
    control_tile_definition_t *wifi = screen_control_find_definition("Wi-Fi");
    const uint8_t brightness = hal_display_get_brightness();

    if(!s_control_state.initialized) {
        return;
    }

    if(s_control_state.brightness_slider != NULL) {
        lv_slider_set_value(s_control_state.brightness_slider, brightness, LV_ANIM_OFF);
    }
    if(s_control_state.brightness_value_label != NULL) {
        lv_label_set_text_fmt(s_control_state.brightness_value_label, "%u%%", brightness);
    }
    if(wifi != NULL) {
        wifi->enabled = wifi_manager_is_enabled();
        screen_control_update_tile_visuals(s_control_state.tiles[0], wifi);
    }

    CONTROL_TILE_DEFINITIONS[1].enabled = time_utils_is_24_hour_enabled();
    screen_control_update_tile_visuals(s_control_state.tiles[1], &CONTROL_TILE_DEFINITIONS[1]);

    if(s_control_state.status_label != NULL) {
        if(wifi_manager_is_connected()) {
            lv_label_set_text(s_control_state.status_label, "Wi-Fi online");
        }
        else if(!wifi_manager_is_enabled()) {
            lv_label_set_text(s_control_state.status_label, "Wi-Fi off");
        }
        else if(wifi_manager_is_configured()) {
            lv_label_set_text(s_control_state.status_label, "Wi-Fi offline");
        }
        else {
            lv_label_set_text(s_control_state.status_label, "Set up Wi-Fi in Settings");
        }
    }
}

static esp_err_t screen_control_build_layout(void)
{
    uint32_t index;
    lv_obj_t *header_label;
    lv_obj_t *status_label;
    lv_obj_t *brightness_slider;

    s_control_state.root = lv_obj_create(NULL);
    if(s_control_state.root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_remove_style_all(s_control_state.root);
    lv_obj_set_size(s_control_state.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_control_state.root, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(s_control_state.root, LV_OPA_COVER, 0);
    header_label = lv_label_create(s_control_state.root);
    status_label = lv_label_create(s_control_state.root);
    if(header_label == NULL || status_label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(header_label, UI_FONT_TIME, 0);
    lv_obj_set_style_text_color(header_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_label_set_text(header_label, "Controls");
    lv_obj_align(header_label, LV_ALIGN_TOP_LEFT, UI_HEADER_SIDE_PADDING_PX, UI_HEADER_TOP_OFFSET_PX);
    lv_obj_set_style_text_font(status_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(status_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_obj_set_width(status_label, UI_SCREEN_WIDTH - (2 * UI_HEADER_SIDE_PADDING_PX));
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(status_label, "Wi-Fi online");
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 56);
    s_control_state.status_label = status_label;

    s_control_state.container = lv_obj_create(s_control_state.root);
    if(s_control_state.container == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_size(s_control_state.container,
                    UI_SCREEN_WIDTH - (2 * UI_EDGE_PADDING_PX),
                    220);
    lv_obj_align(s_control_state.container, LV_ALIGN_TOP_MID, 0, 88);
    lv_obj_set_style_bg_opa(s_control_state.container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_control_state.container, 0, 0);
    lv_obj_set_style_pad_all(s_control_state.container, 0, 0);
    lv_obj_set_style_pad_row(s_control_state.container, 8, 0);
    lv_obj_set_style_pad_column(s_control_state.container, 8, 0);
    lv_obj_set_flex_flow(s_control_state.container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_control_state.container,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(s_control_state.container, LV_OBJ_FLAG_SCROLLABLE);

    for(index = 0; index < SCREEN_CONTROL_TILE_COUNT; ++index) {
        s_control_state.tiles[index] = screen_control_create_tile(s_control_state.container,
                                                                  &CONTROL_TILE_DEFINITIONS[index]);
        if(s_control_state.tiles[index] == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    brightness_slider = lv_slider_create(s_control_state.root);
    if(brightness_slider == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_size(brightness_slider, UI_SCREEN_WIDTH - 96, 12);
    lv_slider_set_range(brightness_slider, 10, 100);
    lv_slider_set_value(brightness_slider, hal_display_get_brightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, UI_COLOR_CONTROL_TILE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider, UI_COLOR_PRIMARY_TEXT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, UI_COLOR_PRIMARY_TEXT, LV_PART_KNOB);
    lv_obj_align(brightness_slider, LV_ALIGN_BOTTOM_LEFT, 48, -42);
    lv_obj_add_event_cb(brightness_slider, screen_control_brightness_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_slider, screen_control_brightness_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(brightness_slider, screen_control_brightness_event_cb, LV_EVENT_PRESS_LOST, NULL);
    s_control_state.brightness_slider = brightness_slider;

    s_control_state.brightness_value_label = lv_label_create(s_control_state.root);
    if(s_control_state.brightness_value_label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_text_font(s_control_state.brightness_value_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(s_control_state.brightness_value_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_obj_align(s_control_state.brightness_value_label, LV_ALIGN_BOTTOM_RIGHT, -24, -60);

    s_control_state.torch_overlay = lv_obj_create(s_control_state.root);
    if(s_control_state.torch_overlay == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_size(s_control_state.torch_overlay, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_control_state.torch_overlay, UI_COLOR_PRIMARY_TEXT, 0);
    lv_obj_set_style_bg_opa(s_control_state.torch_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_control_state.torch_overlay, 0, 0);
    lv_obj_add_event_cb(s_control_state.torch_overlay, screen_control_torch_overlay_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_control_state.torch_overlay, LV_OBJ_FLAG_HIDDEN);

    s_control_state.initialized = true;
    screen_control_refresh();
    return ESP_OK;
}

static lv_obj_t *screen_control_create_tile(lv_obj_t *parent,
                                            control_tile_definition_t *tile_definition)
{
    lv_obj_t *tile;
    lv_obj_t *icon_wrap;
    lv_obj_t *icon_label;
    lv_obj_t *title_label;

    tile = lv_btn_create(parent);
    if(tile == NULL) {
        return NULL;
    }

    lv_obj_set_size(tile, 164, 68);
    ui_styles_apply_glass_surface(tile,
                                  tile_definition->enabled ? lv_color_hex(tile_definition->active_color_hex) : UI_COLOR_CONTROL_TILE,
                                  8);
    lv_obj_set_style_pad_all(tile, 10, 0);
    lv_obj_add_event_cb(tile, screen_control_tile_event_cb, LV_EVENT_CLICKED, tile_definition);

    icon_wrap = lv_obj_create(tile);
    if(icon_wrap == NULL) {
        return NULL;
    }

    lv_obj_set_size(icon_wrap, 36, 36);
    lv_obj_set_style_radius(icon_wrap, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(icon_wrap, 0, 0);
    lv_obj_set_style_shadow_width(icon_wrap, 0, 0);
    lv_obj_set_style_pad_all(icon_wrap, 0, 0);
    lv_obj_align(icon_wrap, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_clear_flag(icon_wrap, LV_OBJ_FLAG_SCROLLABLE);

    icon_label = lv_label_create(icon_wrap);
    if(icon_label == NULL) {
        return NULL;
    }

    lv_obj_set_style_text_font(icon_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(icon_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_label_set_text(icon_label, tile_definition->icon_symbol);
    lv_obj_center(icon_label);

    title_label = lv_label_create(tile);
    if(title_label == NULL) {
        return NULL;
    }

    lv_obj_set_style_text_font(title_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_label_set_text(title_label, tile_definition->label);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 48, 5);

    lv_obj_t *state_label = lv_label_create(tile);
    if(state_label == NULL) {
        return NULL;
    }
    lv_obj_set_style_text_font(state_label, UI_FONT_SOLAR_COMPLICATION, 0);
    lv_obj_set_style_text_color(state_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_obj_align(state_label, LV_ALIGN_BOTTOM_LEFT, 48, -5);

    screen_control_update_tile_visuals(tile, tile_definition);

    return tile;
}

static void screen_control_update_tile_visuals(lv_obj_t *tile, const control_tile_definition_t *tile_definition)
{
    lv_obj_t *icon_wrap;
    lv_obj_t *icon_label;
    lv_obj_t *state_label;

    if(tile == NULL || tile_definition == NULL) {
        return;
    }

    lv_obj_set_style_bg_color(tile,
                              tile_definition->enabled ? lv_color_hex(tile_definition->active_color_hex) : UI_COLOR_CONTROL_TILE,
                              0);

    icon_wrap = lv_obj_get_child(tile, 0);
    if(icon_wrap != NULL) {
        lv_obj_set_style_bg_color(icon_wrap,
                                  tile_definition->enabled ? UI_COLOR_PRIMARY_TEXT : lv_color_hex(0x48484A),
                                  0);
        lv_obj_set_style_bg_opa(icon_wrap, tile_definition->enabled ? LV_OPA_COVER : LV_OPA_90, 0);

        icon_label = lv_obj_get_child(icon_wrap, 0);
        if(icon_label != NULL) {
            lv_obj_set_style_text_color(icon_label,
                                        tile_definition->enabled ? lv_color_hex(tile_definition->active_color_hex) : UI_COLOR_PRIMARY_TEXT,
                                        0);
        }
    }

    state_label = lv_obj_get_child(tile, 2);
    if(state_label != NULL) {
        switch((control_action_t)tile_definition->action) {
            case CONTROL_ACTION_WIFI:
                lv_label_set_text(state_label, wifi_manager_is_connected() ? "Connected" :
                                                (wifi_manager_is_enabled() ? "Connecting" : "Off"));
                break;
            case CONTROL_ACTION_TIME_FORMAT:
                lv_label_set_text(state_label, tile_definition->enabled ? "24 hour" : "12 hour");
                break;
            case CONTROL_ACTION_SYNC:
                lv_label_set_text(state_label, "Time + weather");
                break;
            case CONTROL_ACTION_TORCH:
                lv_label_set_text(state_label, tile_definition->enabled ? "On" : "Off");
                break;
            case CONTROL_ACTION_THEATER:
                lv_label_set_text(state_label, tile_definition->enabled ? "On" : "Off");
                break;
            case CONTROL_ACTION_SETTINGS:
                lv_label_set_text(state_label, "Open");
                break;
        }
    }
}

static void screen_control_tile_event_cb(lv_event_t *event)
{
    control_tile_definition_t *tile_definition;
    lv_obj_t *tile;

    tile_definition = (control_tile_definition_t *)lv_event_get_user_data(event);
    tile = lv_event_get_target(event);

    if(tile_definition == NULL || tile == NULL) {
        return;
    }

    if(tile_definition->action == CONTROL_ACTION_WIFI) {
        if(!wifi_manager_is_configured()) {
            (void)ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            return;
        }
        tile_definition->enabled = !wifi_manager_is_enabled();
        screen_control_update_tile_visuals(tile, tile_definition);
        lv_label_set_text(s_control_state.status_label, tile_definition->enabled ? "Wi-Fi connecting" : "Wi-Fi off");
        if(xTaskCreate(screen_control_wifi_reconnect_task,
                       "wifi_reconnect",
                       4096U,
                       (void *)(uintptr_t)tile_definition->enabled,
                       tskIDLE_PRIORITY + 2U,
                       NULL) != pdPASS) {
            lv_label_set_text(s_control_state.status_label, "Wi-Fi reconnect failed");
        }
        return;
    }

    if(tile_definition->action == CONTROL_ACTION_TIME_FORMAT) {
        tile_definition->enabled = !time_utils_is_24_hour_enabled();
        (void)time_utils_set_24_hour_enabled(tile_definition->enabled);
        screen_control_update_tile_visuals(tile, tile_definition);
        lv_label_set_text(s_control_state.status_label, tile_definition->enabled ? "24-hour time" : "12-hour time");
        return;
    }

    if(tile_definition->action == CONTROL_ACTION_SYNC) {
        if(!wifi_manager_is_connected()) {
            if(!wifi_manager_is_configured()) {
                (void)ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            }
            else {
                lv_label_set_text(s_control_state.status_label, "Connect Wi-Fi to sync");
            }
            return;
        }
        lv_label_set_text(s_control_state.status_label, "Sync requested");
        (void)xTaskCreate(screen_control_sync_task, "control_sync", 4096U, NULL, tskIDLE_PRIORITY + 2U, NULL);
        return;
    }

    if(tile_definition->action == CONTROL_ACTION_TORCH) {
        s_control_state.brightness_before_torch = hal_display_get_brightness();
        tile_definition->enabled = true;
        screen_control_update_tile_visuals(tile, tile_definition);
        lv_obj_clear_flag(s_control_state.torch_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_control_state.torch_overlay);
        (void)hal_display_set_brightness(100U);
        return;
    }

    if(tile_definition->action == CONTROL_ACTION_THEATER) {
        tile_definition->enabled = !tile_definition->enabled;
        if(tile_definition->enabled) {
            s_control_state.brightness_before_theater = hal_display_get_brightness();
            (void)hal_display_set_brightness(10U);
        }
        else {
            (void)hal_display_set_brightness(s_control_state.brightness_before_theater);
        }
        screen_control_update_tile_visuals(tile, tile_definition);
        lv_slider_set_value(s_control_state.brightness_slider,
                            hal_display_get_brightness(),
                            LV_ANIM_OFF);
        lv_label_set_text_fmt(s_control_state.brightness_value_label,
                              "%u%%",
                              hal_display_get_brightness());
        lv_label_set_text(s_control_state.status_label,
                          tile_definition->enabled ? "Theater mode on" : "Theater mode off");
        return;
    }

    if(tile_definition->action == CONTROL_ACTION_SETTINGS) {
        (void)ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
        return;
    }
}

static void screen_control_brightness_event_cb(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_target(event);
    const lv_event_code_t code = lv_event_get_code(event);
    const uint8_t brightness = (uint8_t)lv_slider_get_value(slider);
    control_tile_definition_t *theater = screen_control_find_definition("Theater");

    if(s_control_state.brightness_value_label != NULL) {
        lv_label_set_text_fmt(s_control_state.brightness_value_label, "%u%%", brightness);
    }
    if(code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
        return;
    }

    if(hal_display_set_brightness(brightness) != ESP_OK) {
        screen_control_refresh();
        return;
    }
    if(theater != NULL && theater->enabled) {
        theater->enabled = false;
        screen_control_update_tile_visuals(s_control_state.tiles[4], theater);
    }
}

static void screen_control_torch_overlay_event_cb(lv_event_t *event)
{
    control_tile_definition_t *torch = screen_control_find_definition("Torch");

    lv_obj_add_flag(lv_event_get_target(event), LV_OBJ_FLAG_HIDDEN);
    if(torch != NULL) {
        torch->enabled = false;
        screen_control_update_tile_visuals(s_control_state.tiles[3], torch);
    }
    (void)hal_display_set_brightness(s_control_state.brightness_before_torch > 0U
                                         ? s_control_state.brightness_before_torch
                                         : WATCH_OS_ACTIVE_BRIGHTNESS_PERCENT);
    screen_control_refresh();
}

static void screen_control_wifi_reconnect_task(void *task_parameter)
{
    const bool enabled = ((uintptr_t)task_parameter != 0U);

    (void)wifi_manager_set_enabled(enabled);
    vTaskDelete(NULL);
}

static void screen_control_sync_task(void *task_parameter)
{
    (void)task_parameter;
    (void)time_utils_force_sync(false, 0U);
    (void)weather_service_request_refresh();
    vTaskDelete(NULL);
}

static control_tile_definition_t *screen_control_find_definition(const char *label)
{
    for(uint32_t index = 0; index < (sizeof(CONTROL_TILE_DEFINITIONS) / sizeof(CONTROL_TILE_DEFINITIONS[0])); ++index) {
        if(strcmp(CONTROL_TILE_DEFINITIONS[index].label, label) == 0) {
            return &CONTROL_TILE_DEFINITIONS[index];
        }
    }
    return NULL;
}