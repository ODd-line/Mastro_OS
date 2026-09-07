/**
 * @file screen_settings.h
 * @brief System settings screen.
 */

#ifndef WATCH_OS_SCREEN_SETTINGS_H
#define WATCH_OS_SCREEN_SETTINGS_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and initialize the settings screen.
 *
 * @return ESP_OK on success, or an error if any LVGL object allocation fails.
 */
esp_err_t screen_settings_init(void);

/**
 * @brief Get the settings screen root object.
 *
 * @return Root screen object, or NULL if not initialized.
 */
lv_obj_t *screen_settings_get_root(void);

/**
 * @brief Refresh dynamic settings labels.
 */
void screen_settings_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_SCREEN_SETTINGS_H */