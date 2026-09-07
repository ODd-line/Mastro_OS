/**
 * @file screen_control.h
 * @brief Control center screen.
 */

#ifndef WATCH_OS_SCREEN_CONTROL_H
#define WATCH_OS_SCREEN_CONTROL_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and initialize the control center screen.
 *
 * @return ESP_OK on success, or an error if any LVGL object allocation fails.
 */
esp_err_t screen_control_init(void);

/** Refresh hardware-backed Control Center values and status. */
void screen_control_refresh(void);

/**
 * @brief Get the root LVGL object for the control center screen.
 *
 * @return Root screen object, or NULL if the screen has not been initialized.
 */
lv_obj_t *screen_control_get_root(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_SCREEN_CONTROL_H */