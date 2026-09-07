/**
 * @file hal_touch.h
 * @brief Capacitive touch controller abstraction.
 */

#ifndef WATCH_OS_HAL_TOUCH_H
#define WATCH_OS_HAL_TOUCH_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Last sampled touch controller state. */
typedef struct {
    bool touched;
    uint8_t touch_count;
    uint8_t gesture_id;
    uint16_t x;
    uint16_t y;
} hal_touch_state_t;

/**
 * @brief Initialize the board I2C bus and touch controller through the BSP.
 *
 * The BSP probes the CST820-compatible controller used on V2 and retains
 * support for the original board revision.
 *
 * @return ESP_OK on success, or an ESP-IDF error code if initialization fails.
 */
esp_err_t hal_touch_init(void);

/**
 * @brief Read the latest touch sample from the controller.
 *
 * @param[out] touch_state Destination for the decoded touch state.
 * @return ESP_OK on success, or an ESP-IDF error code if the bus read fails.
 */
esp_err_t hal_touch_read(hal_touch_state_t *touch_state);

/**
 * @brief Release the touch controller device and I2C bus.
 */
void hal_touch_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_HAL_TOUCH_H */