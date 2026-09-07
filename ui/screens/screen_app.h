/**
 * @file screen_app.h
 * @brief Generic app shell for registry-backed apps.
 */

#ifndef WATCH_OS_SCREEN_APP_H
#define WATCH_OS_SCREEN_APP_H

#include "apps/app_registry.h"
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and initialize the generic app shell screen.
 *
 * @return ESP_OK on success, or an error if any LVGL object allocation fails.
 */
esp_err_t screen_app_init(void);

/**
 * @brief Get the generic app shell root object.
 *
 * @return Root screen object, or NULL if not initialized.
 */
lv_obj_t *screen_app_get_root(void);

/**
 * @brief Bind a registered app descriptor to the shell.
 *
 * @param app_descriptor App to present.
 * @return ESP_OK on success.
 */
esp_err_t screen_app_present(const watch_app_descriptor_t *app_descriptor);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_SCREEN_APP_H */