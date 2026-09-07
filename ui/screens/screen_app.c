/**
 * @file screen_app.c
 * @brief Generic app shell for registry-backed apps.
 */

#include "ui/screens/screen_app.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "apps/app_registry.h"
#include "esp_err.h"
#include "ui/ui_defs.h"
#include "ui/ui_manager.h"

typedef struct {
    bool initialized;
    const watch_app_descriptor_t *current_app;
    lv_obj_t *root;
    lv_obj_t *back_button;
    lv_obj_t *title_label;
    lv_obj_t *subtitle_label;
    lv_obj_t *accent_card;
    lv_obj_t *hint_label;
} screen_app_state_t;

static screen_app_state_t s_app_state;

static esp_err_t screen_app_build_layout(void);
static const char *screen_app_get_summary(const char *app_id);
static void screen_app_back_event_cb(lv_event_t *event);

/** {@inheritDoc screen_app_init} */
esp_err_t screen_app_init(void)
{
    if(s_app_state.initialized) {
        return ESP_OK;
    }

    memset(&s_app_state, 0, sizeof(s_app_state));
    return screen_app_build_layout();
}

/** {@inheritDoc screen_app_get_root} */
lv_obj_t *screen_app_get_root(void)
{
    return s_app_state.root;
}

/** {@inheritDoc screen_app_present} */
esp_err_t screen_app_present(const watch_app_descriptor_t *app_descriptor)
{
    if(app_descriptor == NULL || s_app_state.title_label == NULL || s_app_state.subtitle_label == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_app_state.current_app = app_descriptor;
    lv_label_set_text(s_app_state.title_label, app_descriptor->title);
    lv_label_set_text(s_app_state.subtitle_label,
                      app_descriptor->subtitle != NULL ? app_descriptor->subtitle : "Ready on your wrist");
    lv_obj_set_style_bg_color(s_app_state.accent_card, lv_color_hex(app_descriptor->accent_color_hex), 0);
    lv_label_set_text(s_app_state.hint_label, screen_app_get_summary(app_descriptor->app_id));
    return ESP_OK;
}

static esp_err_t screen_app_build_layout(void)
{
    s_app_state.root = lv_obj_create(NULL);
    if(s_app_state.root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_remove_style_all(s_app_state.root);
    lv_obj_set_size(s_app_state.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_app_state.root, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(s_app_state.root, LV_OPA_COVER, 0);

    s_app_state.back_button = lv_btn_create(s_app_state.root);
    if(s_app_state.back_button == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_size(s_app_state.back_button, 56, 56);
    lv_obj_align(s_app_state.back_button, LV_ALIGN_TOP_RIGHT, -12, 12);
    lv_obj_set_style_radius(s_app_state.back_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_app_state.back_button, UI_COLOR_CONTROL_TILE, 0);
    lv_obj_set_style_shadow_width(s_app_state.back_button, 0, 0);
    lv_obj_add_event_cb(s_app_state.back_button, screen_app_back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_icon = lv_label_create(s_app_state.back_button);
    if(back_icon == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_icon, UI_COLOR_PRIMARY_TEXT, 0);
    lv_obj_center(back_icon);

    s_app_state.title_label = lv_label_create(s_app_state.root);
    if(s_app_state.title_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_style_text_font(s_app_state.title_label, UI_FONT_TIME, 0);
    lv_obj_set_style_text_color(s_app_state.title_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_obj_align(s_app_state.title_label, LV_ALIGN_TOP_LEFT, UI_HEADER_SIDE_PADDING_PX, UI_HEADER_TOP_OFFSET_PX);

    s_app_state.subtitle_label = lv_label_create(s_app_state.root);
    if(s_app_state.subtitle_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_width(s_app_state.subtitle_label, UI_SCREEN_WIDTH - (2 * UI_HEADER_SIDE_PADDING_PX));
    lv_obj_set_style_text_font(s_app_state.subtitle_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(s_app_state.subtitle_label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_label_set_long_mode(s_app_state.subtitle_label, LV_LABEL_LONG_WRAP);
    lv_obj_align_to(s_app_state.subtitle_label, s_app_state.title_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, UI_SMALL_GAP_PX);

    s_app_state.accent_card = lv_obj_create(s_app_state.root);
    if(s_app_state.accent_card == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_size(s_app_state.accent_card,
                    UI_SCREEN_WIDTH - (2 * UI_HEADER_SIDE_PADDING_PX),
                    UI_CARD_HEIGHT_PX + UI_CONTROL_TILE_SIZE_PX);
    lv_obj_align(s_app_state.accent_card, LV_ALIGN_CENTER, 0, 28);
    lv_obj_set_style_radius(s_app_state.accent_card, UI_CORNER_RADIUS_PX, 0);
    lv_obj_set_style_border_width(s_app_state.accent_card, 0, 0);
    lv_obj_set_style_shadow_width(s_app_state.accent_card, 0, 0);

    s_app_state.hint_label = lv_label_create(s_app_state.accent_card);
    if(s_app_state.hint_label == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_width(s_app_state.hint_label, UI_SCREEN_WIDTH - (4 * UI_EDGE_PADDING_PX));
    lv_obj_set_style_text_font(s_app_state.hint_label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(s_app_state.hint_label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_label_set_long_mode(s_app_state.hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_center(s_app_state.hint_label);


    s_app_state.initialized = true;
    return ESP_OK;
}

static void screen_app_back_event_cb(lv_event_t *event)
{
    (void)event;
    (void)ui_manager_show_screen(UI_SCREEN_GRID, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

static const char *screen_app_get_summary(const char *app_id)
{
    if(app_id == NULL) return "Ready when you are";
    if(strcmp(app_id, "phone") == 0) return LV_SYMBOL_CALL "  Phone not paired\n\nBluetooth support is not installed";
    if(strcmp(app_id, "music") == 0) return LV_SYMBOL_AUDIO "  Nothing playing\n\nChoose music from your library";
    if(strcmp(app_id, "weather") == 0) return LV_SYMBOL_REFRESH "  Forecast is up to date\n\nConditions refresh automatically";
    if(strcmp(app_id, "messages") == 0) return LV_SYMBOL_EDIT "  You're all caught up\n\nNew messages appear here";
    if(strcmp(app_id, "workout") == 0) return LV_SYMBOL_CHARGE "  Ready to move?\n\nStart a workout from your watch";
    if(strcmp(app_id, "heart") == 0) return LV_SYMBOL_PLUS "  Heart snapshot\n\nWear the watch snugly to measure";
    if(strcmp(app_id, "timer") == 0 || strcmp(app_id, "alarm") == 0) return LV_SYMBOL_BELL "  No active alerts\n\nTap to create one";
    return "Ready on your wrist\n\nSwipe right to return to your apps";
}