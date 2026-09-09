/**
 * @file config.h
 * @brief Board, runtime, and persistence defaults for the watch OS.
 */

#ifndef WATCH_OS_CONFIG_H
#define WATCH_OS_CONFIG_H

#include <stdint.h>

#if defined(__has_include)
#if __has_include("driver/gpio.h")
#include "driver/gpio.h"
#endif
#if __has_include("driver/i2c_master.h")
#include "driver/i2c_master.h"
#endif
#if __has_include("driver/spi_common.h")
#include "driver/spi_common.h"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Display geometry. The physical panel is taller, but the Solar Dial layout is
 * tuned around the 240 px central viewing area. */
#ifndef WATCH_OS_DISPLAY_WIDTH
#define WATCH_OS_DISPLAY_WIDTH                 368U
#endif

#ifndef WATCH_OS_DISPLAY_HEIGHT
#define WATCH_OS_DISPLAY_HEIGHT                448U
#endif

#ifndef WATCH_OS_DISPLAY_ACTIVE_DIAMETER
#define WATCH_OS_DISPLAY_ACTIVE_DIAMETER       240U
#endif

/* SPI AMOLED defaults. These are intended to be overridden per carrier board. */
#ifndef WATCH_OS_DISPLAY_SPI_HOST
#define WATCH_OS_DISPLAY_SPI_HOST              SPI2_HOST
#endif

#ifndef WATCH_OS_DISPLAY_SPI_CLOCK_HZ
#define WATCH_OS_DISPLAY_SPI_CLOCK_HZ          (40U * 1000U * 1000U)
#endif

#ifndef WATCH_OS_DISPLAY_SPI_QUEUE_DEPTH
#define WATCH_OS_DISPLAY_SPI_QUEUE_DEPTH       4U
#endif

#ifndef WATCH_OS_DISPLAY_DRAW_BUFFER_LINES
#define WATCH_OS_DISPLAY_DRAW_BUFFER_LINES     40U
#endif

#ifndef WATCH_OS_DISPLAY_CMD_BITS
#define WATCH_OS_DISPLAY_CMD_BITS              8U
#endif

#ifndef WATCH_OS_DISPLAY_PARAM_BITS
#define WATCH_OS_DISPLAY_PARAM_BITS            8U
#endif

#ifndef WATCH_OS_DISPLAY_PIN_SCLK
#define WATCH_OS_DISPLAY_PIN_SCLK              GPIO_NUM_NC
#endif

#ifndef WATCH_OS_DISPLAY_PIN_MOSI
#define WATCH_OS_DISPLAY_PIN_MOSI              GPIO_NUM_NC
#endif

#ifndef WATCH_OS_DISPLAY_PIN_MISO
#define WATCH_OS_DISPLAY_PIN_MISO              GPIO_NUM_NC
#endif

#ifndef WATCH_OS_DISPLAY_PIN_CS
#define WATCH_OS_DISPLAY_PIN_CS                GPIO_NUM_NC
#endif

#ifndef WATCH_OS_DISPLAY_PIN_DC
#define WATCH_OS_DISPLAY_PIN_DC                GPIO_NUM_NC
#endif

#ifndef WATCH_OS_DISPLAY_PIN_RST
#define WATCH_OS_DISPLAY_PIN_RST               GPIO_NUM_NC
#endif

#ifndef WATCH_OS_DISPLAY_PIN_BKLT
#define WATCH_OS_DISPLAY_PIN_BKLT              GPIO_NUM_NC
#endif

#ifndef WATCH_OS_DISPLAY_GAP_X
#define WATCH_OS_DISPLAY_GAP_X                 0
#endif

#ifndef WATCH_OS_DISPLAY_GAP_Y
#define WATCH_OS_DISPLAY_GAP_Y                 0
#endif

#ifndef WATCH_OS_DISPLAY_MIRROR_X
#define WATCH_OS_DISPLAY_MIRROR_X              0
#endif

#ifndef WATCH_OS_DISPLAY_MIRROR_Y
#define WATCH_OS_DISPLAY_MIRROR_Y              0
#endif

#ifndef WATCH_OS_DISPLAY_SWAP_XY
#define WATCH_OS_DISPLAY_SWAP_XY               0
#endif

#ifndef WATCH_OS_DISPLAY_INVERT_COLOR
#define WATCH_OS_DISPLAY_INVERT_COLOR          0
#endif

/* Touch defaults target a single-point capacitive controller. */
#ifndef WATCH_OS_TOUCH_I2C_PORT
#define WATCH_OS_TOUCH_I2C_PORT                0U
#endif

#ifndef WATCH_OS_TOUCH_I2C_CLOCK_HZ
#define WATCH_OS_TOUCH_I2C_CLOCK_HZ            (400U * 1000U)
#endif

#ifndef WATCH_OS_TOUCH_I2C_ADDRESS
#define WATCH_OS_TOUCH_I2C_ADDRESS             0x15U
#endif

#ifndef WATCH_OS_TOUCH_PIN_SCL
#define WATCH_OS_TOUCH_PIN_SCL                 GPIO_NUM_NC
#endif

#ifndef WATCH_OS_TOUCH_PIN_SDA
#define WATCH_OS_TOUCH_PIN_SDA                 GPIO_NUM_NC
#endif

#ifndef WATCH_OS_TOUCH_PIN_RST
#define WATCH_OS_TOUCH_PIN_RST                 GPIO_NUM_NC
#endif

#ifndef WATCH_OS_TOUCH_PIN_INT
#define WATCH_OS_TOUCH_PIN_INT                 GPIO_NUM_NC
#endif

#ifndef WATCH_OS_TOUCH_RESET_DELAY_MS
#define WATCH_OS_TOUCH_RESET_DELAY_MS          30U
#endif

#ifndef WATCH_OS_TOUCH_POST_RESET_DELAY_MS
#define WATCH_OS_TOUCH_POST_RESET_DELAY_MS     120U
#endif

/* Location, time, and persistence defaults for the Solar Dial face. */
#ifndef WATCH_OS_DEFAULT_LATITUDE
#define WATCH_OS_DEFAULT_LATITUDE              22.3193f
#endif

#ifndef WATCH_OS_DEFAULT_LONGITUDE
#define WATCH_OS_DEFAULT_LONGITUDE             114.1694f
#endif

#ifndef WATCH_OS_DEFAULT_TIMEZONE
#define WATCH_OS_DEFAULT_TIMEZONE              "HKT-8"
#endif

#ifndef WATCH_OS_TIMEZONE_MAX_LENGTH
#define WATCH_OS_TIMEZONE_MAX_LENGTH           64U
#endif

#ifndef WATCH_OS_NTP_SERVER_PRIMARY
#define WATCH_OS_NTP_SERVER_PRIMARY            "pool.ntp.org"
#endif

#ifndef WATCH_OS_NTP_SERVER_SECONDARY
#define WATCH_OS_NTP_SERVER_SECONDARY          "time.nist.gov"
#endif

#ifndef WATCH_OS_NTP_SYNC_TIMEOUT_MS
#define WATCH_OS_NTP_SYNC_TIMEOUT_MS           (15U * 1000U)
#endif

#ifndef WATCH_OS_NTP_SYNC_POLL_MS
#define WATCH_OS_NTP_SYNC_POLL_MS              250U
#endif

#ifndef WATCH_OS_WIFI_MAXIMUM_RETRY
#define WATCH_OS_WIFI_MAXIMUM_RETRY            5U
#endif

#ifndef WATCH_OS_WEATHER_REFRESH_INTERVAL_MS
#define WATCH_OS_WEATHER_REFRESH_INTERVAL_MS   (30U * 60U * 1000U)
#endif

#ifndef WATCH_OS_WEATHER_HTTP_TIMEOUT_MS
#define WATCH_OS_WEATHER_HTTP_TIMEOUT_MS       (8U * 1000U)
#endif

#ifndef WATCH_OS_WEATHER_HTTP_BUFFER_SIZE
#define WATCH_OS_WEATHER_HTTP_BUFFER_SIZE      768U
#endif

#ifndef WATCH_OS_WEATHER_TASK_STACK_SIZE
#define WATCH_OS_WEATHER_TASK_STACK_SIZE       6144U
#endif

#ifndef WATCH_OS_WEATHER_TASK_PRIORITY
#define WATCH_OS_WEATHER_TASK_PRIORITY         4U
#endif

#ifndef WATCH_OS_MIN_VALID_YEAR
#define WATCH_OS_MIN_VALID_YEAR                2024
#endif

#ifndef WATCH_OS_NVS_NAMESPACE
#define WATCH_OS_NVS_NAMESPACE                 "watch_cfg"
#endif

#ifndef WATCH_OS_NVS_KEY_LATITUDE
#define WATCH_OS_NVS_KEY_LATITUDE              "latitude"
#endif

#ifndef WATCH_OS_NVS_KEY_LONGITUDE
#define WATCH_OS_NVS_KEY_LONGITUDE             "longitude"
#endif

#ifndef WATCH_OS_NVS_KEY_TIME_FORMAT
#define WATCH_OS_NVS_KEY_TIME_FORMAT           "time_fmt"
#endif

#ifndef WATCH_OS_NVS_KEY_TIMEZONE
#define WATCH_OS_NVS_KEY_TIMEZONE              "timezone"
#endif

#ifndef WATCH_OS_NVS_KEY_WIFI_SSID
#define WATCH_OS_NVS_KEY_WIFI_SSID             "wifi_ssid"
#endif

#ifndef WATCH_OS_NVS_KEY_WIFI_PASSWORD
#define WATCH_OS_NVS_KEY_WIFI_PASSWORD         "wifi_pass"
#endif

#ifndef WATCH_OS_FACE_REFRESH_PERIOD_MS
#define WATCH_OS_FACE_REFRESH_PERIOD_MS        (60U * 1000U)
#endif

#ifndef WATCH_OS_IDLE_DIM_TIMEOUT_MS
#define WATCH_OS_IDLE_DIM_TIMEOUT_MS           (30U * 1000U)
#endif

#ifndef WATCH_OS_IDLE_SLEEP_TIMEOUT_MS
#define WATCH_OS_IDLE_SLEEP_TIMEOUT_MS         (120U * 1000U)
#endif

#ifndef WATCH_OS_IDLE_DIM_BRIGHTNESS_PERCENT
#define WATCH_OS_IDLE_DIM_BRIGHTNESS_PERCENT   20U
#endif

#ifndef WATCH_OS_ACTIVE_BRIGHTNESS_PERCENT
#define WATCH_OS_ACTIVE_BRIGHTNESS_PERCENT     100U
#endif

#ifndef WATCH_OS_BACKLIGHT_PWM_FREQ_HZ
#define WATCH_OS_BACKLIGHT_PWM_FREQ_HZ         5000U
#endif

#ifndef WATCH_OS_BACKLIGHT_PWM_RESOLUTION_BITS
#define WATCH_OS_BACKLIGHT_PWM_RESOLUTION_BITS 10U
#endif

#ifndef WATCH_OS_BACKLIGHT_LEDC_TIMER
#define WATCH_OS_BACKLIGHT_LEDC_TIMER          LEDC_TIMER_0
#endif

#ifndef WATCH_OS_BACKLIGHT_LEDC_MODE
#define WATCH_OS_BACKLIGHT_LEDC_MODE           LEDC_LOW_SPEED_MODE
#endif

#ifndef WATCH_OS_BACKLIGHT_LEDC_CHANNEL
#define WATCH_OS_BACKLIGHT_LEDC_CHANNEL        LEDC_CHANNEL_0
#endif

#ifndef WATCH_OS_DEFAULT_TIME_FORMAT_24H
#define WATCH_OS_DEFAULT_TIME_FORMAT_24H       1
#endif

#ifndef WATCH_OS_TOUCH_TAP_MAX_MOVEMENT_PX
#define WATCH_OS_TOUCH_TAP_MAX_MOVEMENT_PX     28
#endif

#ifndef WATCH_OS_TOUCH_TAP_MAX_DURATION_MS
#define WATCH_OS_TOUCH_TAP_MAX_DURATION_MS     500U
#endif

#ifndef WATCH_OS_TOUCH_LONG_PRESS_MS
#define WATCH_OS_TOUCH_LONG_PRESS_MS           700U
#endif

#ifndef WATCH_OS_FACE_CENTER_TAP_RADIUS_PX
#define WATCH_OS_FACE_CENTER_TAP_RADIUS_PX     70
#endif

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_CONFIG_H */