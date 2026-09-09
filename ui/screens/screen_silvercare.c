/**
 * @file screen_silvercare.c
 * @brief Native SilverCare companion status screen.
 */

#include "ui/screens/screen_silvercare.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "ui/ui_defs.h"
#include "ui/ui_manager.h"
#include "ui/ui_styles.h"

#define SILVERCARE_ACCENT_COLOR       lv_color_hex(0x24A878UL)
#define SILVERCARE_PANEL_COLOR        lv_color_hex(0x173D32UL)
#define SILVERCARE_SLOT_SIZE_PX       66
#define SILVERCARE_SLOT_GAP_PX        8

typedef struct {
    bool initialized;
    lv_obj_t *root;
} screen_silvercare_state_t;

static screen_silvercare_state_t s_silvercare_state;
static const lv_coord_t s_silvercare_grid_columns[] = {
    SILVERCARE_SLOT_SIZE_PX,
    SILVERCARE_SLOT_SIZE_PX,
    SILVERCARE_SLOT_SIZE_PX,
    SILVERCARE_SLOT_SIZE_PX,
    LV_GRID_TEMPLATE_LAST
};
static const lv_coord_t s_silvercare_grid_rows[] = {
    SILVERCARE_SLOT_SIZE_PX,
    SILVERCARE_SLOT_SIZE_PX,
    SILVERCARE_SLOT_SIZE_PX,
    LV_GRID_TEMPLATE_LAST
};

static esp_err_t screen_silvercare_build_layout(void);
static void screen_silvercare_back_event_cb(lv_event_t *event);

esp_err_t screen_silvercare_init(void)
{
    if(s_silvercare_state.initialized) {
        return ESP_OK;
    }

    memset(&s_silvercare_state, 0, sizeof(s_silvercare_state));
    return screen_silvercare_build_layout();
}

lv_obj_t *screen_silvercare_get_root(void)
{
    return s_silvercare_state.root;
}

static esp_err_t screen_silvercare_build_layout(void)
{
    lv_obj_t *back_button;
    lv_obj_t *label;
    lv_obj_t *status_panel;
    lv_obj_t *slot_grid;
    uint8_t slot_index;

    s_silvercare_state.root = lv_obj_create(NULL);
    if(s_silvercare_state.root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_remove_style_all(s_silvercare_state.root);
    lv_obj_set_size(s_silvercare_state.root, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_silvercare_state.root, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(s_silvercare_state.root, LV_OPA_COVER, 0);

    label = lv_label_create(s_silvercare_state.root);
    if(label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_label_set_text(label, "SilverCare");
    lv_obj_set_style_text_font(label, UI_FONT_TIME, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, UI_HEADER_SIDE_PADDING_PX, UI_HEADER_TOP_OFFSET_PX);

    back_button = lv_btn_create(s_silvercare_state.root);
    if(back_button == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_size(back_button, 48, 48);
    lv_obj_align(back_button, LV_ALIGN_TOP_RIGHT, -12, 12);
    ui_styles_apply_glass_surface(back_button, UI_COLOR_CONTROL_TILE, LV_RADIUS_CIRCLE);
    lv_obj_add_event_cb(back_button, screen_silvercare_back_event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(back_button);
    if(label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_label_set_text(label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_obj_center(label);

    status_panel = lv_obj_create(s_silvercare_state.root);
    if(status_panel == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_size(status_panel, UI_SCREEN_WIDTH - (2 * UI_HEADER_SIDE_PADDING_PX), 76);
    lv_obj_align(status_panel, LV_ALIGN_TOP_MID, 0, 76);
    ui_styles_apply_glass_surface(status_panel, SILVERCARE_PANEL_COLOR, 14);
    lv_obj_set_style_pad_all(status_panel, 14, 0);
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(status_panel);
    if(label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_label_set_text(label, "UNPAIRED");
    lv_obj_set_style_text_font(label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(label, SILVERCARE_ACCENT_COLOR, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    label = lv_label_create(status_panel);
    if(label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_label_set_text(label, "Phone link is not installed\nCompartment state unavailable");
    lv_obj_set_style_text_font(label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_PRIMARY_TEXT, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    label = lv_label_create(s_silvercare_state.root);
    if(label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_label_set_text(label, "12 compartments");
    lv_obj_set_style_text_font(label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, UI_HEADER_SIDE_PADDING_PX, 164);

    slot_grid = lv_obj_create(s_silvercare_state.root);
    if(slot_grid == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_size(slot_grid,
                    (4 * SILVERCARE_SLOT_SIZE_PX) + (3 * SILVERCARE_SLOT_GAP_PX),
                    (3 * SILVERCARE_SLOT_SIZE_PX) + (2 * SILVERCARE_SLOT_GAP_PX));
    lv_obj_align(slot_grid, LV_ALIGN_TOP_MID, 0, 190);
    lv_obj_set_style_bg_opa(slot_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(slot_grid, 0, 0);
    lv_obj_set_style_pad_all(slot_grid, 0, 0);
    lv_obj_set_style_pad_row(slot_grid, SILVERCARE_SLOT_GAP_PX, 0);
    lv_obj_set_style_pad_column(slot_grid, SILVERCARE_SLOT_GAP_PX, 0);
    lv_obj_set_grid_dsc_array(slot_grid, s_silvercare_grid_columns, s_silvercare_grid_rows);
    lv_obj_clear_flag(slot_grid, LV_OBJ_FLAG_SCROLLABLE);

    for(slot_index = 0U; slot_index < 12U; ++slot_index) {
        lv_obj_t *slot = lv_obj_create(slot_grid);
        if(slot == NULL) {
            return ESP_ERR_NO_MEM;
        }
        lv_obj_set_grid_cell(slot,
                             LV_GRID_ALIGN_STRETCH,
                             slot_index % 4U,
                             1,
                             LV_GRID_ALIGN_STRETCH,
                             slot_index / 4U,
                             1);
        ui_styles_apply_glass_surface(slot, UI_COLOR_CONTROL_TILE, 10);
        lv_obj_set_style_pad_all(slot, 0, 0);
        lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);

        label = lv_label_create(slot);
        if(label == NULL) {
            return ESP_ERR_NO_MEM;
        }
        lv_label_set_text_fmt(label, "%u\n--", (unsigned int)(slot_index + 1U));
        lv_obj_set_style_text_font(label, UI_FONT_LABEL, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label, UI_COLOR_PRIMARY_TEXT, 0);
        lv_obj_center(label);
    }

    label = lv_label_create(s_silvercare_state.root);
    if(label == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_width(label, UI_SCREEN_WIDTH - (2 * UI_HEADER_SIDE_PADDING_PX));
    lv_label_set_text(label, "Lid state never proves medicine was taken");
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, UI_FONT_LABEL, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_SECONDARY_TEXT, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -12);

    s_silvercare_state.initialized = true;
    return ESP_OK;
}

static void screen_silvercare_back_event_cb(lv_event_t *event)
{
    (void)event;
    (void)ui_manager_show_screen(UI_SCREEN_GRID, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}
