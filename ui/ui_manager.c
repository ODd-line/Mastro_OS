/**
 * @file ui_manager.c
 * @brief Central screen registry, pointer input device, and swipe navigation.
 */

#include "ui/ui_manager.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "apps/app_registry.h"
#include "config.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/hal_display.h"
#include "hal/hal_touch.h"
#include "ui/screens/screen_app.h"
#include "ui/screens/screen_control.h"
#include "ui/screens/screen_face.h"
#include "ui/screens/screen_grid.h"
#include "ui/screens/screen_settings.h"
#include "utils/time_utils.h"

typedef struct {
    bool pressed;
    bool long_press_handled;
    bool consume_release;
    lv_point_t start_point;
    lv_point_t last_point;
    uint32_t press_start_ms;
    ui_swipe_direction_t pending_swipe;
} ui_gesture_state_t;

typedef struct {
    bool initialized;
    ui_screen_id_t active_screen;
    lv_obj_t *screens[UI_SCREEN_COUNT];
    lv_indev_t *touch_input_device;
    lv_indev_drv_t input_driver;
    lv_timer_t *gesture_timer;
    uint32_t last_activity_ms;
    bool display_dimmed;
    ui_gesture_state_t gesture_state;
} ui_manager_state_t;

typedef esp_err_t (*ui_manager_screen_prepare_fn_t)(void);
typedef lv_obj_t *(*ui_manager_screen_root_fn_t)(void);

typedef struct {
    ui_manager_screen_root_fn_t get_root;
    ui_manager_screen_prepare_fn_t prepare_for_show;
} ui_manager_screen_descriptor_t;

static const char *TAG = "ui_manager";
static ui_manager_state_t s_ui_manager;
static esp_err_t ui_manager_prepare_face(void);
static esp_err_t ui_manager_prepare_grid(void);
static esp_err_t ui_manager_prepare_settings(void);
static esp_err_t ui_manager_prepare_control(void);
static esp_err_t ui_manager_prepare_noop(void);
static const ui_manager_screen_descriptor_t s_screen_descriptors[UI_SCREEN_COUNT] = {
    [UI_SCREEN_FACE] = {
        .get_root = screen_face_get_root,
        .prepare_for_show = ui_manager_prepare_face,
    },
    [UI_SCREEN_GRID] = {
        .get_root = screen_grid_get_root,
        .prepare_for_show = ui_manager_prepare_grid,
    },
    [UI_SCREEN_CONTROL] = {
        .get_root = screen_control_get_root,
        .prepare_for_show = ui_manager_prepare_control,
    },
    [UI_SCREEN_SETTINGS] = {
        .get_root = screen_settings_get_root,
        .prepare_for_show = ui_manager_prepare_settings,
    },
    [UI_SCREEN_APP] = {
        .get_root = screen_app_get_root,
        .prepare_for_show = ui_manager_prepare_noop,
    },
};

static esp_err_t ui_manager_register_screens(void);
static lv_obj_t *ui_manager_get_screen_root(ui_screen_id_t screen_id);
static esp_err_t ui_manager_prepare_screen(ui_screen_id_t screen_id);
static esp_err_t ui_manager_record_activity(void);
static bool ui_manager_is_center_tap(const lv_point_t *point);
static bool ui_manager_is_tap_gesture(const ui_gesture_state_t *gesture_state, uint32_t release_timestamp_ms);
static esp_err_t ui_manager_handle_face_tap(void);
static esp_err_t ui_manager_handle_face_long_press(void);
static void ui_manager_touch_read_cb(lv_indev_drv_t *driver, lv_indev_data_t *data);
static void ui_manager_gesture_timer_cb(lv_timer_t *timer);
static ui_swipe_direction_t ui_manager_detect_swipe(const ui_gesture_state_t *gesture_state,
                                                    uint32_t release_timestamp_ms);
static esp_err_t ui_manager_handle_swipe(ui_swipe_direction_t direction);

/** {@inheritDoc ui_manager_init} */
esp_err_t ui_manager_init(void)
{
    esp_err_t ret = ESP_OK;

    if(s_ui_manager.initialized) {
        return ESP_OK;
    }

    memset(&s_ui_manager, 0, sizeof(s_ui_manager));
    s_ui_manager.active_screen = UI_SCREEN_FACE;
    s_ui_manager.last_activity_ms = lv_tick_get();

    ESP_GOTO_ON_ERROR(ui_manager_register_screens(), cleanup, TAG, "screen registration failed");

    lv_indev_drv_init(&s_ui_manager.input_driver);
    s_ui_manager.input_driver.type = LV_INDEV_TYPE_POINTER;
    s_ui_manager.input_driver.read_cb = ui_manager_touch_read_cb;
    s_ui_manager.touch_input_device = lv_indev_drv_register(&s_ui_manager.input_driver);
    if(s_ui_manager.touch_input_device == NULL) {
        ret = ESP_ERR_NO_MEM;
        ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "lv_indev_drv_register failed");
    }

    s_ui_manager.gesture_timer = lv_timer_create(ui_manager_gesture_timer_cb, LV_INDEV_DEF_READ_PERIOD, NULL);
    if(s_ui_manager.gesture_timer == NULL) {
        ret = ESP_ERR_NO_MEM;
        ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "lv_timer_create failed");
    }

    ESP_GOTO_ON_ERROR(ui_manager_show_screen(UI_SCREEN_FACE, LV_SCR_LOAD_ANIM_NONE), cleanup, TAG, "initial screen load failed");

    s_ui_manager.initialized = true;
    return ESP_OK;

cleanup:
    ui_manager_deinit();
    return ret;
}

/** {@inheritDoc ui_manager_deinit} */
void ui_manager_deinit(void)
{
    if(s_ui_manager.gesture_timer != NULL) {
        lv_timer_del(s_ui_manager.gesture_timer);
        s_ui_manager.gesture_timer = NULL;
    }

    memset(&s_ui_manager, 0, sizeof(s_ui_manager));
}

/** {@inheritDoc ui_manager_show_screen} */
esp_err_t ui_manager_show_screen(ui_screen_id_t screen_id, lv_scr_load_anim_t animation)
{
    lv_obj_t *screen_root = ui_manager_get_screen_root(screen_id);

    if(screen_root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(ui_manager_prepare_screen(screen_id), TAG, "screen preparation failed");

    lv_scr_load_anim(screen_root,
                     animation,
                     UI_SCREEN_TRANSITION_MS,
                     UI_SCREEN_TRANSITION_DELAY_MS,
                     false);
    s_ui_manager.active_screen = screen_id;
    return ESP_OK;
}

/** {@inheritDoc ui_manager_open_app} */
esp_err_t ui_manager_open_app(const watch_app_descriptor_t *app_descriptor)
{
    if(app_descriptor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch(app_descriptor->target) {
        case WATCH_APP_TARGET_SETTINGS:
            return ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_TOP);

        case WATCH_APP_TARGET_SHELL:
        default:
            ESP_RETURN_ON_ERROR(screen_app_present(app_descriptor), TAG, "screen_app_present failed");
            return ui_manager_show_screen(UI_SCREEN_APP, LV_SCR_LOAD_ANIM_MOVE_TOP);
    }
}

/** {@inheritDoc ui_manager_reload_apps} */
esp_err_t ui_manager_reload_apps(void)
{
    ESP_RETURN_ON_ERROR(ui_manager_prepare_grid(), TAG, "screen_grid_refresh failed");
    ESP_RETURN_ON_ERROR(ui_manager_prepare_settings(), TAG, "screen_settings_refresh failed");
    return ESP_OK;
}

/** {@inheritDoc ui_manager_notify_user_activity} */
esp_err_t ui_manager_notify_user_activity(void)
{
    return ui_manager_record_activity();
}

/** {@inheritDoc ui_manager_get_active_screen} */
ui_screen_id_t ui_manager_get_active_screen(void)
{
    return s_ui_manager.active_screen;
}

static esp_err_t ui_manager_register_screens(void)
{
    ESP_RETURN_ON_ERROR(screen_face_init(), TAG, "screen_face_init failed");
    ESP_RETURN_ON_ERROR(screen_grid_init(), TAG, "screen_grid_init failed");
    ESP_RETURN_ON_ERROR(screen_control_init(), TAG, "screen_control_init failed");
    ESP_RETURN_ON_ERROR(screen_settings_init(), TAG, "screen_settings_init failed");
    ESP_RETURN_ON_ERROR(screen_app_init(), TAG, "screen_app_init failed");

    s_ui_manager.screens[UI_SCREEN_FACE] = s_screen_descriptors[UI_SCREEN_FACE].get_root();
    s_ui_manager.screens[UI_SCREEN_GRID] = s_screen_descriptors[UI_SCREEN_GRID].get_root();
    s_ui_manager.screens[UI_SCREEN_CONTROL] = s_screen_descriptors[UI_SCREEN_CONTROL].get_root();
    s_ui_manager.screens[UI_SCREEN_SETTINGS] = s_screen_descriptors[UI_SCREEN_SETTINGS].get_root();
    s_ui_manager.screens[UI_SCREEN_APP] = s_screen_descriptors[UI_SCREEN_APP].get_root();

    if(s_ui_manager.screens[UI_SCREEN_FACE] == NULL ||
       s_ui_manager.screens[UI_SCREEN_GRID] == NULL ||
       s_ui_manager.screens[UI_SCREEN_CONTROL] == NULL ||
       s_ui_manager.screens[UI_SCREEN_SETTINGS] == NULL ||
       s_ui_manager.screens[UI_SCREEN_APP] == NULL) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static lv_obj_t *ui_manager_get_screen_root(ui_screen_id_t screen_id)
{
    if(screen_id >= UI_SCREEN_COUNT) {
        return NULL;
    }

    return s_ui_manager.screens[screen_id];
}

static esp_err_t ui_manager_prepare_screen(ui_screen_id_t screen_id)
{
    if(screen_id >= UI_SCREEN_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if(s_screen_descriptors[screen_id].prepare_for_show == NULL) {
        return ESP_OK;
    }

    return s_screen_descriptors[screen_id].prepare_for_show();
}

static esp_err_t ui_manager_prepare_face(void)
{
    screen_face_refresh();
    return ESP_OK;
}

static esp_err_t ui_manager_prepare_grid(void)
{
    return ESP_OK;
}

static esp_err_t ui_manager_prepare_settings(void)
{
    screen_settings_refresh();
    return ESP_OK;
}

static esp_err_t ui_manager_prepare_control(void)
{
    screen_control_refresh();
    return ESP_OK;
}

static esp_err_t ui_manager_prepare_noop(void)
{
    return ESP_OK;
}

static esp_err_t ui_manager_record_activity(void)
{
    s_ui_manager.last_activity_ms = lv_tick_get();

    if(hal_display_is_sleeping()) {
        ESP_RETURN_ON_ERROR(hal_display_set_sleeping(false), TAG, "display wake failed");
    }

    if(s_ui_manager.display_dimmed) {
        ESP_RETURN_ON_ERROR(hal_display_set_dimmed(false), TAG, "display restore failed");
        s_ui_manager.display_dimmed = false;
    }

    return ESP_OK;
}

static bool ui_manager_is_center_tap(const lv_point_t *point)
{
    if(point == NULL) {
        return false;
    }

    const int32_t delta_x = (int32_t)point->x - UI_SOLAR_DIAL_CENTER_X_PX;
    const int32_t delta_y = (int32_t)point->y - UI_SOLAR_DIAL_CENTER_Y_PX;
    const int32_t radius = WATCH_OS_FACE_CENTER_TAP_RADIUS_PX;

    return ((delta_x * delta_x) + (delta_y * delta_y)) <= (radius * radius);
}

static bool ui_manager_is_tap_gesture(const ui_gesture_state_t *gesture_state, uint32_t release_timestamp_ms)
{
    const int32_t delta_x = (int32_t)gesture_state->last_point.x - (int32_t)gesture_state->start_point.x;
    const int32_t delta_y = (int32_t)gesture_state->last_point.y - (int32_t)gesture_state->start_point.y;
    const uint32_t duration_ms = release_timestamp_ms - gesture_state->press_start_ms;

    return (!gesture_state->long_press_handled &&
            duration_ms <= WATCH_OS_TOUCH_TAP_MAX_DURATION_MS &&
            LV_ABS(delta_x) <= WATCH_OS_TOUCH_TAP_MAX_MOVEMENT_PX &&
            LV_ABS(delta_y) <= WATCH_OS_TOUCH_TAP_MAX_MOVEMENT_PX);
}

static esp_err_t ui_manager_handle_face_tap(void)
{
    ESP_RETURN_ON_ERROR(time_utils_set_24_hour_enabled(!time_utils_is_24_hour_enabled()),
                        TAG,
                        "time format toggle failed");
    screen_face_refresh();
    screen_settings_refresh();
    return ESP_OK;
}

static esp_err_t ui_manager_handle_face_long_press(void)
{
    return ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_TOP);
}

static void ui_manager_touch_read_cb(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    hal_touch_state_t touch_state;
    ui_gesture_state_t *gesture_state = &s_ui_manager.gesture_state;
    const uint32_t timestamp_ms = lv_tick_get();
    const bool display_was_sleeping = hal_display_is_sleeping();

    (void)driver;

    if(hal_touch_read(&touch_state) != ESP_OK || !touch_state.touched) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point = gesture_state->last_point;

        if(gesture_state->consume_release) {
            gesture_state->consume_release = false;
            return;
        }

        if(gesture_state->pressed) {
            if(!gesture_state->consume_release) {
                if(ui_manager_is_tap_gesture(gesture_state, timestamp_ms) &&
                   s_ui_manager.active_screen == UI_SCREEN_FACE &&
                   ui_manager_is_center_tap(&gesture_state->last_point)) {
                    (void)ui_manager_handle_face_tap();
                }
                else {
                    gesture_state->pending_swipe = ui_manager_detect_swipe(gesture_state, timestamp_ms);
                }
            }

            gesture_state->pressed = false;
            gesture_state->long_press_handled = false;
            gesture_state->consume_release = false;
        }

        return;
    }

    if(ui_manager_record_activity() != ESP_OK) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if(display_was_sleeping) {
        gesture_state->pressed = false;
        gesture_state->consume_release = true;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if(gesture_state->consume_release) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = (lv_coord_t)touch_state.x;
    data->point.y = (lv_coord_t)touch_state.y;
    gesture_state->last_point = data->point;

    if(!gesture_state->pressed) {
        gesture_state->pressed = true;
        gesture_state->long_press_handled = false;
        gesture_state->start_point = data->point;
        gesture_state->press_start_ms = timestamp_ms;
        gesture_state->pending_swipe = UI_SWIPE_NONE;
    }
}

static void ui_manager_gesture_timer_cb(lv_timer_t *timer)
{
    ui_swipe_direction_t direction;
    ui_gesture_state_t *gesture_state = &s_ui_manager.gesture_state;
    const uint32_t now_ms = lv_tick_get();

    (void)timer;

    if(!hal_display_is_sleeping() && !s_ui_manager.display_dimmed &&
       (now_ms - s_ui_manager.last_activity_ms) >= WATCH_OS_IDLE_DIM_TIMEOUT_MS) {
        if(hal_display_set_dimmed(true) == ESP_OK) {
            s_ui_manager.display_dimmed = true;
        }
    }

    if(!hal_display_is_sleeping() &&
       (now_ms - s_ui_manager.last_activity_ms) >= WATCH_OS_IDLE_SLEEP_TIMEOUT_MS) {
        (void)hal_display_set_sleeping(true);
        s_ui_manager.display_dimmed = false;
    }

    if(gesture_state->pressed && !gesture_state->long_press_handled &&
       s_ui_manager.active_screen == UI_SCREEN_FACE &&
       (now_ms - gesture_state->press_start_ms) >= WATCH_OS_TOUCH_LONG_PRESS_MS &&
       ui_manager_is_center_tap(&gesture_state->last_point)) {
        gesture_state->long_press_handled = true;
        gesture_state->pending_swipe = UI_SWIPE_NONE;
        (void)ui_manager_handle_face_long_press();
        return;
    }

    direction = s_ui_manager.gesture_state.pending_swipe;
    if(direction == UI_SWIPE_NONE) {
        return;
    }

    s_ui_manager.gesture_state.pending_swipe = UI_SWIPE_NONE;
    ui_manager_handle_swipe(direction);
}

static ui_swipe_direction_t ui_manager_detect_swipe(const ui_gesture_state_t *gesture_state,
                                                    uint32_t release_timestamp_ms)
{
    const int32_t delta_x = (int32_t)gesture_state->last_point.x - (int32_t)gesture_state->start_point.x;
    const int32_t delta_y = (int32_t)gesture_state->last_point.y - (int32_t)gesture_state->start_point.y;
    const uint32_t duration_ms = release_timestamp_ms - gesture_state->press_start_ms;
    const int32_t abs_delta_x = LV_ABS(delta_x);
    const int32_t abs_delta_y = LV_ABS(delta_y);

    if(duration_ms > UI_GESTURE_MAX_DURATION_MS) {
        return UI_SWIPE_NONE;
    }

    if(abs_delta_y >= UI_GESTURE_MIN_DISTANCE_PX && abs_delta_x <= UI_GESTURE_MAX_CROSS_AXIS_PX) {
        return (delta_y < 0) ? UI_SWIPE_UP : UI_SWIPE_DOWN;
    }

    if(abs_delta_x >= UI_GESTURE_MIN_DISTANCE_PX && abs_delta_y <= UI_GESTURE_MAX_CROSS_AXIS_PX) {
        return (delta_x < 0) ? UI_SWIPE_LEFT : UI_SWIPE_RIGHT;
    }

    return UI_SWIPE_NONE;
}

static esp_err_t ui_manager_handle_swipe(ui_swipe_direction_t direction)
{
    switch(s_ui_manager.active_screen) {
        case UI_SCREEN_FACE:
            if(direction == UI_SWIPE_UP) {
                return ui_manager_show_screen(UI_SCREEN_GRID, LV_SCR_LOAD_ANIM_MOVE_TOP);
            }

            if(direction == UI_SWIPE_DOWN) {
                return ui_manager_show_screen(UI_SCREEN_CONTROL, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
            break;

        case UI_SCREEN_GRID:
            if(direction == UI_SWIPE_DOWN) {
                return ui_manager_show_screen(UI_SCREEN_FACE, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
            break;

        case UI_SCREEN_CONTROL:
            if(direction == UI_SWIPE_UP) {
                return ui_manager_show_screen(UI_SCREEN_FACE, LV_SCR_LOAD_ANIM_MOVE_TOP);
            }
            break;

        case UI_SCREEN_SETTINGS:
            if(direction == UI_SWIPE_DOWN || direction == UI_SWIPE_RIGHT) {
                return ui_manager_show_screen(UI_SCREEN_GRID, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
            break;

        case UI_SCREEN_APP: {
            const lv_point_t *start = &s_ui_manager.gesture_state.start_point;
            const bool edge_exit =
                (direction == UI_SWIPE_RIGHT && start->x <= UI_GESTURE_EDGE_ZONE_PX) ||
                (direction == UI_SWIPE_LEFT && start->x >= (UI_SCREEN_WIDTH - UI_GESTURE_EDGE_ZONE_PX)) ||
                (direction == UI_SWIPE_DOWN && start->y <= UI_GESTURE_EDGE_ZONE_PX) ||
                (direction == UI_SWIPE_UP && start->y >= (UI_SCREEN_HEIGHT - UI_GESTURE_EDGE_ZONE_PX));

            if(edge_exit) {
                return ui_manager_show_screen(UI_SCREEN_GRID, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            }
            break;
        }

        default:
            break;
    }

    return ESP_OK;
}