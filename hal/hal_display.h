/**
 * @file hal_display.h
 * @brief LVGL display bridge for the smartwatch AMOLED panel.
 */

#ifndef WATCH_OS_HAL_DISPLAY_H
#define WATCH_OS_HAL_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Waveshare CO5300 panel and LVGL display driver.
 *
 * The official board support package owns QSPI setup, panel commands, revision
 * offset, and command-based AMOLED brightness control.
 *
 * @return ESP_OK on success, or an ESP-IDF error code if initialization fails.
 */
esp_err_t hal_display_init(void);

/**
 * @brief Release panel IO objects, draw buffers, and the LVGL display driver.
 */
void hal_display_deinit(void);

/**
 * @brief Update the user-selected brightness.
 *
 * @param brightness_percent Brightness from 0 to 100.
 * @return ESP_OK on success.
 */
esp_err_t hal_display_set_brightness(uint8_t brightness_percent);

/**
 * @brief Get the stored user-selected brightness.
 *
 * @return Brightness from 0 to 100.
 */
uint8_t hal_display_get_brightness(void);

/**
 * @brief Temporarily dim or restore the display.
 *
 * @param dimmed True to apply the idle dim level.
 * @return ESP_OK on success.
 */
esp_err_t hal_display_set_dimmed(bool dimmed);

/**
 * @brief Put the display to sleep or wake it.
 *
 * @param sleeping True to turn the panel off.
 * @return ESP_OK on success.
 */
esp_err_t hal_display_set_sleeping(bool sleeping);

/**
 * @brief Check whether the panel is currently sleeping.
 *
 * @return True if sleeping.
 */
bool hal_display_is_sleeping(void);

/**
 * @brief Get the registered LVGL display instance.
 *
 * @return Pointer to the LVGL display, or NULL if the display is not ready.
 */
lv_disp_t *hal_display_get_lvgl_display(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_HAL_DISPLAY_H */