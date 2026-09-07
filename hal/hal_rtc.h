/**
 * @file hal_rtc.h
 * @brief PCF85063 real-time clock access.
 */

#ifndef WATCH_OS_HAL_RTC_H
#define WATCH_OS_HAL_RTC_H

#include <time.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hal_rtc_init(void);
esp_err_t hal_rtc_get_time(struct tm *time_out);
esp_err_t hal_rtc_set_time(const struct tm *time_value);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_HAL_RTC_H */
