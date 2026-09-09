/**
 * @file app_registry.h
 * @brief Registry for launcher-visible smartwatch apps.
 */

#ifndef WATCH_OS_APP_REGISTRY_H
#define WATCH_OS_APP_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Destination used when a launcher tile is activated. */
typedef enum {
    WATCH_APP_TARGET_SHELL = 0,
    WATCH_APP_TARGET_SETTINGS,
    WATCH_APP_TARGET_SILVERCARE
} watch_app_target_t;

/**
 * @brief Launcher metadata for an app.
 *
 * To add a new app, define one of these descriptors in your app module and
 * register it with app_registry_register() before ui_manager_init() runs.
 */
typedef struct {
    const char *app_id;
    const char *title;
    const char *subtitle;
    uint32_t accent_color_hex;
    watch_app_target_t target;
    bool visible_in_grid;
} watch_app_descriptor_t;

/**
 * @brief Initialize the registry storage.
 *
 * @return ESP_OK on success.
 */
esp_err_t app_registry_init(void);

/**
 * @brief Register a launcher-visible or internal app descriptor.
 *
 * @param descriptor App metadata that must remain valid for the life of the UI.
 * @return ESP_OK on success, or an error if the registry is full or the app ID is duplicated.
 */
esp_err_t app_registry_register(const watch_app_descriptor_t *descriptor);

/**
 * @brief Register the built-in smartwatch apps.
 *
 * @return ESP_OK on success.
 */
esp_err_t app_registry_register_builtin_apps(void);

/**
 * @brief Get the number of apps visible in the launcher grid.
 *
 * @return Visible app count.
 */
size_t app_registry_get_visible_count(void);

/**
 * @brief Get a visible app descriptor by launcher index.
 *
 * @param visible_index Zero-based visible-app index.
 * @return Descriptor pointer, or NULL if out of range.
 */
const watch_app_descriptor_t *app_registry_get_visible_at(size_t visible_index);

/**
 * @brief Look up an app by ID.
 *
 * @param app_id Stable app identifier.
 * @return Descriptor pointer, or NULL if not found.
 */
const watch_app_descriptor_t *app_registry_get_by_id(const char *app_id);

/**
 * @brief Get the total number of registered apps.
 *
 * @return Total app count.
 */
size_t app_registry_get_total_count(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_APP_REGISTRY_H */