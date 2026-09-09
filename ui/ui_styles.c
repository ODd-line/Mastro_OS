#include "ui/ui_styles.h"

#include "ui/ui_defs.h"

void ui_styles_apply_glass_surface(lv_obj_t *object, lv_color_t background_color, lv_coord_t radius)
{
    if(object == NULL) {
        return;
    }

    lv_obj_set_style_bg_color(object, background_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(object, UI_GLASS_BACKGROUND_OPA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(object, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(object, UI_GLASS_BORDER_WIDTH_PX, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(object, UI_COLOR_GLASS_EDGE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(object, UI_GLASS_BORDER_OPA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(object, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_opa(object, UI_GLASS_PRESSED_OPA, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(object, UI_GLASS_BORDER_PRESSED_OPA, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(object, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
}