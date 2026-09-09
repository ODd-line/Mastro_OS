/**
 * @file screen_silvercare.h
 * @brief SilverCare companion status screen.
 */

#ifndef WATCH_OS_SCREEN_SILVERCARE_H
#define WATCH_OS_SCREEN_SILVERCARE_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t screen_silvercare_init(void);
lv_obj_t *screen_silvercare_get_root(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_SCREEN_SILVERCARE_H */
