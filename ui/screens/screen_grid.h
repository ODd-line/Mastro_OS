/**
 * @file screen_grid.h
 * @brief App grid screen.
 */

#ifndef WATCH_OS_SCREEN_GRID_H
#define WATCH_OS_SCREEN_GRID_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and initialize the app grid screen.
 *
 * @return ESP_OK on success, or an error if any LVGL object allocation fails.
 */
esp_err_t screen_grid_init(void);

/**
 * @brief Get the root LVGL object for the app grid screen.
 *
 * @return Root screen object, or NULL if the screen has not been initialized.
 */
lv_obj_t *screen_grid_get_root(void);

/**
 * @brief Rebuild the app grid from the current registry contents.
 *
 * @return ESP_OK on success.
 */
esp_err_t screen_grid_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_SCREEN_GRID_H */