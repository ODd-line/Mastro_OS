/**
 * @file screen_grid.c
 * @brief App grid implementation.
 */

#include "ui/screens/screen_grid.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include "apps/app_registry.h"
#include "esp_check.h"
#include "esp_err.h"
#include "ui/ui_manager.h"
#include "ui/ui_defs.h"

typedef struct {
    int16_t q;
    int16_t r;
} screen_grid_hex_coord_t;

typedef struct {
    lv_obj_t *button;
    lv_coord_t base_center_x;
    lv_coord_t base_center_y;
    lv_coord_t base_size;
} screen_grid_icon_state_t;

typedef struct {
    bool initialized;
    lv_obj_t *root;
    lv_obj_t *scroll_container;
    screen_grid_icon_state_t icons[UI_MAX_REGISTERED_APPS];
    size_t icon_count;
} screen_grid_state_t;

static screen_grid_state_t s_grid_state;

static esp_err_t screen_grid_build_layout(void);
static esp_err_t screen_grid_populate(void);
static void screen_grid_get_cluster_slot(size_t cluster_slot, screen_grid_hex_coord_t *hex_coord, uint8_t *ring_level);
static void screen_grid_get_visual_slot(size_t visible_index,
                                        lv_coord_t *center_x,
                                        lv_coord_t *center_y,
                                        lv_coord_t *icon_size);
static lv_obj_t *screen_grid_create_icon(lv_obj_t *parent,
                                         const watch_app_descriptor_t *app_descriptor,
                                         lv_coord_t center_x,
                                         lv_coord_t center_y,
                                         lv_coord_t icon_size);
static const char *screen_grid_get_app_symbol(const watch_app_descriptor_t *app_descriptor);
static void screen_grid_build_glyph_text(const char *title, char *glyph_text, size_t glyph_text_size);
static void screen_grid_update_focus_scaling(void);
static void screen_grid_scroll_event_cb(lv_event_t *event);
static void screen_grid_app_event_cb(lv_event_t *event);

/** {@inheritDoc screen_grid_init} */
esp_err_t screen_grid_init(void)
{
    if(s_grid_state.initialized) {
        return screen_grid_refresh();
    }

    memset(&s_grid_state, 0, sizeof(s_grid_state));
    return screen_grid_build_layout();
}

/** {@inheritDoc screen_grid_refresh} */
esp_err_t screen_grid_refresh(void)
{
    return screen_grid_populate();
}

/** {@inheritDoc screen_grid_get_root} */
lv_obj_t *screen_grid_get_root(void)
{
    return s_grid_state.root;
}

static esp_err_t screen_grid_build_layout(void)
{
    s_grid_state.root = lv_obj_create(NULL);
    if(s_grid_state.root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_remove_style_all(s_grid_state.root);
    lv_obj_set_size(s_grid_state.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_grid_state.root, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(s_grid_state.root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_grid_state.root, 0, 0);

    s_grid_state.scroll_container = lv_obj_create(s_grid_state.root);
    if(s_grid_state.scroll_container == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_remove_style_all(s_grid_state.scroll_container);
    lv_obj_set_size(s_grid_state.scroll_container, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_set_style_bg_opa(s_grid_state.scroll_container, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(s_grid_state.scroll_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_grid_state.scroll_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_grid_state.scroll_container, screen_grid_scroll_event_cb, LV_EVENT_SCROLL, NULL);

    ESP_RETURN_ON_ERROR(screen_grid_populate(), "screen_grid", "grid populate failed");

    s_grid_state.initialized = true;
    return ESP_OK;
}

static esp_err_t screen_grid_populate(void)
{
    size_t index;
    size_t app_count;
    size_t page_count;
    lv_coord_t content_height;

    if(s_grid_state.scroll_container == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    lv_obj_clean(s_grid_state.scroll_container);
    memset(s_grid_state.icons, 0, sizeof(s_grid_state.icons));
    s_grid_state.icon_count = 0U;
    app_count = app_registry_get_visible_count();

    for(index = 0; index < app_count; ++index) {
        const watch_app_descriptor_t *app_descriptor = app_registry_get_visible_at(index);
        lv_coord_t center_x;
        lv_coord_t center_y;
        lv_coord_t icon_size;

        if(app_descriptor == NULL) {
            continue;
        }

        screen_grid_get_visual_slot(index, &center_x, &center_y, &icon_size);

        if(screen_grid_create_icon(s_grid_state.scroll_container,
                                   app_descriptor,
                                   center_x,
                                   center_y,
                                   icon_size) == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    page_count = (app_count + (UI_APP_GRID_CLUSTER_CAPACITY - 1U)) / UI_APP_GRID_CLUSTER_CAPACITY;
    if(page_count == 0U) {
        page_count = 1U;
    }

    content_height = UI_APP_GRID_TOP_OFFSET_PX +
                     ((lv_coord_t)(page_count - 1U) * UI_APP_GRID_CLUSTER_PITCH_PX) +
                     UI_APP_GRID_CENTER_Y_PX +
                     UI_APP_ICON_SIZE_FOCUS_PX +
                     UI_APP_GRID_BOTTOM_PADDING_PX;
    lv_obj_set_scrollbar_mode(s_grid_state.scroll_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(s_grid_state.scroll_container, content_height, 0);
    lv_obj_scroll_to_y(s_grid_state.scroll_container, 0, LV_ANIM_OFF);
    screen_grid_update_focus_scaling();

    return ESP_OK;
}

static void screen_grid_get_cluster_slot(size_t cluster_slot, screen_grid_hex_coord_t *hex_coord, uint8_t *ring_level)
{
    static const screen_grid_hex_coord_t directions[] = {
        { 1, 0 },
        { 0, 1 },
        { -1, 1 },
        { -1, 0 },
        { 0, -1 },
        { 1, -1 },
    };

    size_t remaining;
    uint8_t ring = 0U;
    uint8_t side;

    if(hex_coord == NULL || ring_level == NULL) {
        return;
    }

    if(cluster_slot == 0U) {
        hex_coord->q = 0;
        hex_coord->r = 0;
        *ring_level = 0U;
        return;
    }

    remaining = cluster_slot - 1U;
    ring = 1U;
    while(remaining >= (size_t)(6U * ring)) {
        remaining -= (size_t)(6U * ring);
        ++ring;
    }

    hex_coord->q = 0;
    hex_coord->r = -(int16_t)ring;

    for(side = 0U; side < 6U; ++side) {
        size_t side_steps = ring;
        if(remaining < side_steps) {
            hex_coord->q += (int16_t)(directions[side].q * (int16_t)remaining);
            hex_coord->r += (int16_t)(directions[side].r * (int16_t)remaining);
            break;
        }

        hex_coord->q += (int16_t)(directions[side].q * (int16_t)side_steps);
        hex_coord->r += (int16_t)(directions[side].r * (int16_t)side_steps);
        remaining -= side_steps;
    }

    *ring_level = ring;
}

static void screen_grid_get_visual_slot(size_t visible_index,
                                        lv_coord_t *center_x,
                                        lv_coord_t *center_y,
                                        lv_coord_t *icon_size)
{
    screen_grid_hex_coord_t hex_coord;
    uint8_t ring_level = 0U;
    const size_t page_index = visible_index / UI_APP_GRID_CLUSTER_CAPACITY;
    const size_t cluster_slot = visible_index % UI_APP_GRID_CLUSTER_CAPACITY;

    screen_grid_get_cluster_slot(cluster_slot, &hex_coord, &ring_level);

    if(center_x != NULL) {
        *center_x = (lv_coord_t)(UI_APP_GRID_CENTER_X_PX +
                                 (hex_coord.q * UI_APP_GRID_HEX_X_STEP_PX) +
                                 ((hex_coord.r * UI_APP_GRID_HEX_X_STEP_PX) / 2));
    }

    if(center_y != NULL) {
        *center_y = (lv_coord_t)(UI_APP_GRID_TOP_OFFSET_PX +
                                 UI_APP_GRID_CENTER_Y_PX +
                                 ((lv_coord_t)page_index * UI_APP_GRID_CLUSTER_PITCH_PX) +
                                 (hex_coord.r * UI_APP_GRID_HEX_Y_STEP_PX));
    }

    if(icon_size != NULL) {
        if(ring_level == 0U) {
            *icon_size = UI_APP_ICON_SIZE_FOCUS_PX;
        }
        else if(ring_level == 1U) {
            *icon_size = UI_APP_ICON_SIZE_MID_PX;
        }
        else {
            *icon_size = UI_APP_ICON_SIZE_EDGE_PX;
        }
    }
}

static lv_obj_t *screen_grid_create_icon(lv_obj_t *parent,
                                         const watch_app_descriptor_t *app_descriptor,
                                         lv_coord_t center_x,
                                         lv_coord_t center_y,
                                         lv_coord_t icon_size)
{
    lv_obj_t *icon_button;
    lv_obj_t *icon_label;
    char glyph_text[UI_APP_ICON_GLYPH_CHARS + 1U];
    const char *icon_text;

    icon_button = lv_btn_create(parent);
    if(icon_button == NULL) {
        return NULL;
    }

    /* Circular bubbles and a larger center app better match the watchOS honeycomb launcher. */
    lv_obj_set_size(icon_button, icon_size, icon_size);
    lv_obj_set_style_radius(icon_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(icon_button, lv_color_hex(app_descriptor->accent_color_hex), 0);
    lv_obj_set_style_bg_opa(icon_button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(icon_button, 0, 0);
    lv_obj_set_style_shadow_width(icon_button, 0, 0);
    lv_obj_set_style_pad_all(icon_button, 0, 0);
    lv_obj_add_flag(icon_button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_pos(icon_button,
                   center_x - (icon_size / 2),
                   center_y - (icon_size / 2));
    lv_obj_add_event_cb(icon_button, screen_grid_app_event_cb, LV_EVENT_CLICKED, (void *)app_descriptor);

    if(s_grid_state.icon_count < UI_MAX_REGISTERED_APPS) {
        s_grid_state.icons[s_grid_state.icon_count].button = icon_button;
        s_grid_state.icons[s_grid_state.icon_count].base_center_x = center_x;
        s_grid_state.icons[s_grid_state.icon_count].base_center_y = center_y;
        s_grid_state.icons[s_grid_state.icon_count].base_size = icon_size;
        ++s_grid_state.icon_count;
    }

    icon_label = lv_label_create(icon_button);
    if(icon_label == NULL) {
        return NULL;
    }

    icon_text = screen_grid_get_app_symbol(app_descriptor);
    if(icon_text == NULL) {
        screen_grid_build_glyph_text(app_descriptor->title, glyph_text, sizeof(glyph_text));
        icon_text = glyph_text;
    }
    lv_obj_set_style_text_font(icon_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(icon_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_label_set_text(icon_label, icon_text);
    lv_obj_center(icon_label);

    return icon_button;
}

static const char *screen_grid_get_app_symbol(const watch_app_descriptor_t *app_descriptor)
{
    if(app_descriptor == NULL || app_descriptor->app_id == NULL) {
        return NULL;
    }

    if(strcmp(app_descriptor->app_id, "phone") == 0) return LV_SYMBOL_CALL;
    if(strcmp(app_descriptor->app_id, "music") == 0) return LV_SYMBOL_AUDIO;
    if(strcmp(app_descriptor->app_id, "maps") == 0) return LV_SYMBOL_GPS;
    if(strcmp(app_descriptor->app_id, "workout") == 0) return LV_SYMBOL_CHARGE;
    if(strcmp(app_descriptor->app_id, "heart") == 0) return LV_SYMBOL_PLUS;
    if(strcmp(app_descriptor->app_id, "home") == 0) return LV_SYMBOL_HOME;
    if(strcmp(app_descriptor->app_id, "timer") == 0) return LV_SYMBOL_LOOP;
    if(strcmp(app_descriptor->app_id, "sleep") == 0) return LV_SYMBOL_EYE_CLOSE;
    if(strcmp(app_descriptor->app_id, "wallet") == 0) return LV_SYMBOL_SAVE;
    if(strcmp(app_descriptor->app_id, "settings") == 0) return LV_SYMBOL_SETTINGS;
    if(strcmp(app_descriptor->app_id, "weather") == 0) return LV_SYMBOL_REFRESH;
    if(strcmp(app_descriptor->app_id, "messages") == 0) return LV_SYMBOL_EDIT;
    if(strcmp(app_descriptor->app_id, "calendar") == 0) return LV_SYMBOL_LIST;
    if(strcmp(app_descriptor->app_id, "alarm") == 0) return LV_SYMBOL_BELL;
    if(strcmp(app_descriptor->app_id, "camera") == 0) return LV_SYMBOL_IMAGE;
    if(strcmp(app_descriptor->app_id, "compass") == 0) return LV_SYMBOL_GPS;
    if(strcmp(app_descriptor->app_id, "activity") == 0) return LV_SYMBOL_LOOP;
    if(strcmp(app_descriptor->app_id, "calculator") == 0) return LV_SYMBOL_PLUS;
    if(strcmp(app_descriptor->app_id, "find") == 0) return LV_SYMBOL_EYE_OPEN;
    return NULL;
}

static void screen_grid_build_glyph_text(const char *title, char *glyph_text, size_t glyph_text_size)
{
    size_t source_index = 0U;
    size_t glyph_index = 0U;

    if(glyph_text == NULL || glyph_text_size == 0U) {
        return;
    }

    while(title != NULL && title[source_index] != '\0' && glyph_index < (glyph_text_size - 1U)) {
        if(isalnum((unsigned char)title[source_index])) {
            glyph_text[glyph_index] = (char)toupper((unsigned char)title[source_index]);
            ++glyph_index;
        }
        ++source_index;
    }

    if(glyph_index == 0U) {
        glyph_text[glyph_index++] = '?';
    }

    glyph_text[glyph_index] = '\0';
}

static void screen_grid_update_focus_scaling(void)
{
    size_t index;
    const lv_coord_t scroll_offset_y = lv_obj_get_scroll_y(s_grid_state.scroll_container);
    const lv_coord_t viewport_center_x = UI_APP_GRID_CENTER_X_PX;
    const lv_coord_t viewport_center_y = scroll_offset_y + (UI_SCREEN_HEIGHT / 2);

    for(index = 0; index < s_grid_state.icon_count; ++index) {
        screen_grid_icon_state_t *icon_state = &s_grid_state.icons[index];
        const lv_coord_t delta_x = icon_state->base_center_x - viewport_center_x;
        const lv_coord_t delta_y = icon_state->base_center_y - viewport_center_y;
        const lv_coord_t abs_delta_x = LV_ABS(delta_x);
        const lv_coord_t abs_delta_y = LV_ABS(delta_y);
        const lv_coord_t radial_distance = LV_MAX(abs_delta_x, abs_delta_y) + (LV_MIN(abs_delta_x, abs_delta_y) / 2);
        const lv_coord_t clamped_distance = LV_MIN(radial_distance, UI_APP_GRID_FOCUS_RADIUS_PX);
        const lv_coord_t shrink_amount = (lv_coord_t)((clamped_distance * UI_APP_GRID_SIDE_SHRINK_PX) /
                                                      UI_APP_GRID_FOCUS_RADIUS_PX);
        const lv_coord_t focused_size = icon_state->base_size - shrink_amount;
        const lv_coord_t center_pull = (lv_coord_t)(((UI_APP_GRID_FOCUS_RADIUS_PX - clamped_distance) * UI_APP_GRID_CENTER_PULL_PX) /
                                                    UI_APP_GRID_FOCUS_RADIUS_PX);
        const lv_coord_t adjusted_center_x = icon_state->base_center_x -
                                             ((delta_x > 0) ? center_pull : ((delta_x < 0) ? -center_pull : 0));
        const lv_coord_t adjusted_center_y = icon_state->base_center_y -
                                             ((delta_y > 0) ? (center_pull / 2) : ((delta_y < 0) ? -(center_pull / 2) : 0));
        lv_obj_t *icon_label;

        if(icon_state->button == NULL) {
            continue;
        }

        lv_obj_set_size(icon_state->button, focused_size, focused_size);
        lv_obj_set_pos(icon_state->button,
                       adjusted_center_x - (focused_size / 2),
                       adjusted_center_y - (focused_size / 2));
        lv_obj_set_style_opa(icon_state->button,
                             (radial_distance < UI_APP_GRID_LABEL_FADE_RADIUS_PX) ? LV_OPA_COVER : LV_OPA_80,
                             0);

        icon_label = lv_obj_get_child(icon_state->button, 0);
        if(icon_label != NULL) {
            lv_obj_set_style_text_opa(icon_label,
                                      (radial_distance < UI_APP_GRID_LABEL_FADE_RADIUS_PX) ? LV_OPA_COVER : LV_OPA_70,
                                      0);
        }
    }
}

static void screen_grid_scroll_event_cb(lv_event_t *event)
{
    (void)event;
    screen_grid_update_focus_scaling();
}

static void screen_grid_app_event_cb(lv_event_t *event)
{
    const watch_app_descriptor_t *app_descriptor = (const watch_app_descriptor_t *)lv_event_get_user_data(event);

    if(app_descriptor == NULL) {
        return;
    }

    (void)ui_manager_open_app(app_descriptor);
}