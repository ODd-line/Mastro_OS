/**
 * @file ui_manager.h
 * @brief Central navigation, input registration, and gesture handling.
 */

#ifndef WATCH_OS_UI_MANAGER_H
#define WATCH_OS_UI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "apps/app_registry.h"
#include "esp_err.h"
#include "lvgl.h"
#include "ui/ui_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI manager, screens, LVGL input device, and navigation state.
 *
 * @return ESP_OK on success, or an ESP-IDF error code if setup fails.
 */
esp_err_t ui_manager_init(void);

/**
 * @brief Deinitialize timers and input devices owned by the UI manager.
 */
void ui_manager_deinit(void);

/**
 * @brief Load one of the registered smartwatch screens with animation.
 *
 * @param screen_id Destination screen identifier.
 * @param animation Transition animation.
 * @return ESP_OK on success, or an error if the screen is unavailable.
 */
esp_err_t ui_manager_show_screen(ui_screen_id_t screen_id, lv_scr_load_anim_t animation);

/**
 * @brief Open an app registered in the launcher catalog.
 *
 * @param app_descriptor App to present.
 * @return ESP_OK on success.
 */
esp_err_t ui_manager_open_app(const watch_app_descriptor_t *app_descriptor);

/**
 * @brief Refresh app-driven UI surfaces after registering or removing apps.
 *
 * @return ESP_OK on success.
 */
esp_err_t ui_manager_reload_apps(void);

/**
 * @brief Mark the watch as active and restore the display if it dimmed or slept.
 *
 * This supports non-touch wake sources such as buttons or motion sensors.
 *
 * @return ESP_OK on success.
 */
esp_err_t ui_manager_notify_user_activity(void);

/**
 * @brief Get the currently active screen identifier.
 *
 * @return Active screen.
 */
ui_screen_id_t ui_manager_get_active_screen(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_UI_MANAGER_H */