/**
 * @file screen_face.h
 * @brief Main watch face screen.
 */

#ifndef WATCH_OS_SCREEN_FACE_H
#define WATCH_OS_SCREEN_FACE_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and initialize the watch face screen.
 *
 * @return ESP_OK on success, or an error if any LVGL object allocation fails.
 */
esp_err_t screen_face_init(void);

/**
 * @brief Get the root LVGL object for the watch face screen.
 *
 * @return Root screen object, or NULL if the screen has not been initialized.
 */
lv_obj_t *screen_face_get_root(void);

/**
 * @brief Refresh the watch face time and date labels immediately.
 */
void screen_face_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_SCREEN_FACE_H */