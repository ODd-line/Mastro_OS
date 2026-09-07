/**
 * @file ui_defs.h
 * @brief Shared visual, layout, gesture, and navigation definitions.
 */

#ifndef WATCH_OS_UI_DEFS_H
#define WATCH_OS_UI_DEFS_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Allow board-specific build flags to override the panel geometry. */
#ifndef UI_SCREEN_WIDTH
#define UI_SCREEN_WIDTH                  WATCH_OS_DISPLAY_WIDTH
#endif

#ifndef UI_SCREEN_HEIGHT
#define UI_SCREEN_HEIGHT                 WATCH_OS_DISPLAY_HEIGHT
#endif

/* Board defaults are supplied by config.h but preserved behind existing UI_*
 * aliases so the current HAL and screen code keeps compiling unchanged. */
#ifndef UI_DISPLAY_SPI_HOST
#define UI_DISPLAY_SPI_HOST              WATCH_OS_DISPLAY_SPI_HOST
#endif

#ifndef UI_DISPLAY_SPI_CLOCK_HZ
#define UI_DISPLAY_SPI_CLOCK_HZ          WATCH_OS_DISPLAY_SPI_CLOCK_HZ
#endif

#ifndef UI_DISPLAY_SPI_QUEUE_DEPTH
#define UI_DISPLAY_SPI_QUEUE_DEPTH       WATCH_OS_DISPLAY_SPI_QUEUE_DEPTH
#endif

#ifndef UI_DISPLAY_DRAW_BUFFER_LINES
#define UI_DISPLAY_DRAW_BUFFER_LINES     WATCH_OS_DISPLAY_DRAW_BUFFER_LINES
#endif

#ifndef UI_DISPLAY_CMD_BITS
#define UI_DISPLAY_CMD_BITS              WATCH_OS_DISPLAY_CMD_BITS
#endif

#ifndef UI_DISPLAY_PARAM_BITS
#define UI_DISPLAY_PARAM_BITS            WATCH_OS_DISPLAY_PARAM_BITS
#endif

#ifndef UI_DISPLAY_PIN_SCLK
#define UI_DISPLAY_PIN_SCLK              WATCH_OS_DISPLAY_PIN_SCLK
#endif

#ifndef UI_DISPLAY_PIN_MOSI
#define UI_DISPLAY_PIN_MOSI              WATCH_OS_DISPLAY_PIN_MOSI
#endif

#ifndef UI_DISPLAY_PIN_MISO
#define UI_DISPLAY_PIN_MISO              WATCH_OS_DISPLAY_PIN_MISO
#endif

#ifndef UI_DISPLAY_PIN_CS
#define UI_DISPLAY_PIN_CS                WATCH_OS_DISPLAY_PIN_CS
#endif

#ifndef UI_DISPLAY_PIN_DC
#define UI_DISPLAY_PIN_DC                WATCH_OS_DISPLAY_PIN_DC
#endif

#ifndef UI_DISPLAY_PIN_RST
#define UI_DISPLAY_PIN_RST               WATCH_OS_DISPLAY_PIN_RST
#endif

#ifndef UI_DISPLAY_PIN_BKLT
#define UI_DISPLAY_PIN_BKLT              WATCH_OS_DISPLAY_PIN_BKLT
#endif

#ifndef UI_DISPLAY_GAP_X
#define UI_DISPLAY_GAP_X                 WATCH_OS_DISPLAY_GAP_X
#endif

#ifndef UI_DISPLAY_GAP_Y
#define UI_DISPLAY_GAP_Y                 WATCH_OS_DISPLAY_GAP_Y
#endif

#ifndef UI_DISPLAY_MIRROR_X
#define UI_DISPLAY_MIRROR_X              WATCH_OS_DISPLAY_MIRROR_X
#endif

#ifndef UI_DISPLAY_MIRROR_Y
#define UI_DISPLAY_MIRROR_Y              WATCH_OS_DISPLAY_MIRROR_Y
#endif

#ifndef UI_DISPLAY_SWAP_XY
#define UI_DISPLAY_SWAP_XY               WATCH_OS_DISPLAY_SWAP_XY
#endif

#ifndef UI_DISPLAY_INVERT_COLOR
#define UI_DISPLAY_INVERT_COLOR          WATCH_OS_DISPLAY_INVERT_COLOR
#endif

/* Touch defaults target a common single-point capacitive controller. */
#ifndef UI_TOUCH_I2C_PORT
#define UI_TOUCH_I2C_PORT                WATCH_OS_TOUCH_I2C_PORT
#endif

#ifndef UI_TOUCH_I2C_CLOCK_HZ
#define UI_TOUCH_I2C_CLOCK_HZ            WATCH_OS_TOUCH_I2C_CLOCK_HZ
#endif

#ifndef UI_TOUCH_I2C_ADDRESS
#define UI_TOUCH_I2C_ADDRESS             WATCH_OS_TOUCH_I2C_ADDRESS
#endif

#ifndef UI_TOUCH_PIN_SCL
#define UI_TOUCH_PIN_SCL                 WATCH_OS_TOUCH_PIN_SCL
#endif

#ifndef UI_TOUCH_PIN_SDA
#define UI_TOUCH_PIN_SDA                 WATCH_OS_TOUCH_PIN_SDA
#endif

#ifndef UI_TOUCH_PIN_RST
#define UI_TOUCH_PIN_RST                 WATCH_OS_TOUCH_PIN_RST
#endif

#ifndef UI_TOUCH_PIN_INT
#define UI_TOUCH_PIN_INT                 WATCH_OS_TOUCH_PIN_INT
#endif

#ifndef UI_TOUCH_RESET_DELAY_MS
#define UI_TOUCH_RESET_DELAY_MS          WATCH_OS_TOUCH_RESET_DELAY_MS
#endif

#ifndef UI_TOUCH_POST_RESET_DELAY_MS
#define UI_TOUCH_POST_RESET_DELAY_MS     WATCH_OS_TOUCH_POST_RESET_DELAY_MS
#endif

/* AMOLED palette. Pure black is retained for shell screens, while the face uses
 * a very subtle midnight radial gradient for depth. */
#define UI_COLOR_BACKGROUND_HEX          0x000000UL
#define UI_COLOR_CONTROL_CENTER_HEX      0x1C1C1EUL
#define UI_COLOR_ACCENT_RED_HEX          0xFF3B30UL
#define UI_COLOR_ACCENT_GREEN_HEX        0x34C759UL
#define UI_COLOR_ACCENT_BLUE_HEX         0x007AFFUL
#define UI_COLOR_PRIMARY_TEXT_HEX        0xFFFFFFUL
#define UI_COLOR_SECONDARY_TEXT_HEX      0xA1A1A6UL
#define UI_COLOR_CONTROL_TILE_HEX        0x2C2C2EUL
#define UI_COLOR_SOLAR_SKY_CENTER_HEX    0x0B0B15UL
#define UI_COLOR_SOLAR_SKY_EDGE_HEX      0x000000UL
#define UI_COLOR_SOLAR_DAY_ARC_HEX       0xFF9F0AUL
#define UI_COLOR_SOLAR_DAY_ARC_START_HEX UI_COLOR_SOLAR_DAY_ARC_HEX
#define UI_COLOR_SOLAR_NIGHT_ARC_HEX     0x333333UL
#define UI_COLOR_SOLAR_SUN_CORE_HEX      0xFFD60AUL
#define UI_COLOR_SOLAR_SUN_EDGE_HEX      UI_COLOR_SOLAR_SUN_CORE_HEX
#define UI_COLOR_SOLAR_COMPLICATION_HEX  0x8E8E93UL
#define UI_COLOR_RING_TIME_HEX           0x4D4D4DUL
#define UI_COLOR_RING_TECTONIC_HEX       0x3A6EA5UL
#define UI_COLOR_RING_SOLAR_HEX          0xFF9F0AUL

#define UI_COLOR_BACKGROUND              lv_color_hex(UI_COLOR_BACKGROUND_HEX)
#define UI_COLOR_CONTROL_CENTER          lv_color_hex(UI_COLOR_CONTROL_CENTER_HEX)
#define UI_COLOR_ACCENT_RED               lv_color_hex(UI_COLOR_ACCENT_RED_HEX)
#define UI_COLOR_ACCENT_GREEN             lv_color_hex(UI_COLOR_ACCENT_GREEN_HEX)
#define UI_COLOR_ACCENT_BLUE              lv_color_hex(UI_COLOR_ACCENT_BLUE_HEX)
#define UI_COLOR_PRIMARY_TEXT             lv_color_hex(UI_COLOR_PRIMARY_TEXT_HEX)
#define UI_COLOR_SECONDARY_TEXT           lv_color_hex(UI_COLOR_SECONDARY_TEXT_HEX)
#define UI_COLOR_CONTROL_TILE             lv_color_hex(UI_COLOR_CONTROL_TILE_HEX)
#define UI_COLOR_SOLAR_SKY_CENTER         lv_color_hex(UI_COLOR_SOLAR_SKY_CENTER_HEX)
#define UI_COLOR_SOLAR_SKY_EDGE           lv_color_hex(UI_COLOR_SOLAR_SKY_EDGE_HEX)
#define UI_COLOR_SOLAR_DAY_ARC            lv_color_hex(UI_COLOR_SOLAR_DAY_ARC_HEX)
#define UI_COLOR_SOLAR_DAY_ARC_START      lv_color_hex(UI_COLOR_SOLAR_DAY_ARC_START_HEX)
#define UI_COLOR_SOLAR_NIGHT_ARC          lv_color_hex(UI_COLOR_SOLAR_NIGHT_ARC_HEX)
#define UI_COLOR_SOLAR_SUN_CORE           lv_color_hex(UI_COLOR_SOLAR_SUN_CORE_HEX)
#define UI_COLOR_SOLAR_SUN_EDGE           lv_color_hex(UI_COLOR_SOLAR_SUN_EDGE_HEX)
#define UI_COLOR_SOLAR_COMPLICATION       lv_color_hex(UI_COLOR_SOLAR_COMPLICATION_HEX)
#define UI_COLOR_RING_TIME                lv_color_hex(UI_COLOR_RING_TIME_HEX)
#define UI_COLOR_RING_TECTONIC            lv_color_hex(UI_COLOR_RING_TECTONIC_HEX)
#define UI_COLOR_RING_SOLAR               lv_color_hex(UI_COLOR_RING_SOLAR_HEX)

/* Typography aliases keep font choices consistent across screens. */
#define UI_FONT_TIME                      (&lv_font_montserrat_28)
#define UI_FONT_LABEL                     (&lv_font_montserrat_14)
#if defined(LV_FONT_MONTSERRAT_36) && LV_FONT_MONTSERRAT_36
#define UI_FONT_SOLAR_TIME                (&lv_font_montserrat_36)
#elif defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
#define UI_FONT_SOLAR_TIME                (&lv_font_montserrat_32)
#else
#define UI_FONT_SOLAR_TIME                UI_FONT_TIME
#endif
#if defined(LV_FONT_MONTSERRAT_12) && LV_FONT_MONTSERRAT_12
#define UI_FONT_SOLAR_COMPLICATION        (&lv_font_montserrat_12)
#else
#define UI_FONT_SOLAR_COMPLICATION        UI_FONT_LABEL
#endif
#define UI_FONT_SOLAR_DATE                UI_FONT_LABEL

/* Shared spacing and component geometry, in physical display pixels. */
#define UI_EDGE_PADDING_PX                16
#define UI_ITEM_GAP_PX                    12
#define UI_SMALL_GAP_PX                   8
#define UI_CORNER_RADIUS_PX               18
#define UI_CONTROL_TILE_SIZE_PX           72
#define UI_CONTROL_ICON_WRAP_SIZE_PX      36
#define UI_CONTROL_ICON_SIZE_PX           18
#define UI_APP_ICON_SIZE_PX               56
#define UI_APP_ICON_RADIUS_PX             18
#define UI_STATUS_ICON_SIZE_PX            24
#define UI_FACE_TOP_PADDING_PX            22
#define UI_FACE_SIDE_PADDING_PX           20
#define UI_SOLAR_DIAL_CENTER_AREA_PX      WATCH_OS_DISPLAY_ACTIVE_DIAMETER
#define UI_RING_TIME_RADIUS_PX            58
#define UI_RING_TIME_WIDTH_PX             2
#define UI_RING_TECTONIC_RADIUS_PX        74
#define UI_RING_TECTONIC_WIDTH_PX         3
#define UI_RING_SOLAR_RADIUS_PX           90
#define UI_RING_SOLAR_WIDTH_PX            6
#define UI_RING_TIME_OPA                  LV_OPA_40
#define UI_RING_TECTONIC_OPA              LV_OPA_60
#define UI_RING_SOLAR_OPA                 LV_OPA_COVER
#define UI_SOLAR_DIAL_OUTER_RING_RADIUS_PX UI_RING_SOLAR_RADIUS_PX
#define UI_SOLAR_DIAL_OUTER_RING_WIDTH_PX UI_RING_SOLAR_WIDTH_PX
#define UI_SOLAR_DIAL_OUTER_RING_OPA      UI_RING_SOLAR_OPA
#define UI_SOLAR_DIAL_INNER_RING_RADIUS_PX UI_RING_TECTONIC_RADIUS_PX
#define UI_SOLAR_DIAL_INNER_RING_WIDTH_PX UI_RING_TECTONIC_WIDTH_PX
#define UI_SOLAR_DIAL_INNER_RING_OPA      UI_RING_TECTONIC_OPA
#define UI_SOLAR_DIAL_SUN_RADIUS_PX       7
#define UI_SOLAR_DIAL_SUN_DIAMETER_PX     (UI_SOLAR_DIAL_SUN_RADIUS_PX * 2)
#define UI_SOLAR_DIAL_SUN_SHADOW_WIDTH_PX 10
#define UI_SOLAR_DIAL_SUN_SHADOW_OPA      LV_OPA_50
#define UI_SOLAR_DIAL_CENTER_X_PX         (UI_SCREEN_WIDTH / 2)
#define UI_SOLAR_DIAL_CENTER_Y_PX         176
#define UI_SOLAR_DIAL_CARDINAL_MARKER_COUNT 4U
#define UI_SOLAR_DIAL_CARDINAL_MARKER_SHORT_PX 2
#define UI_SOLAR_DIAL_CARDINAL_MARKER_LONG_PX 5
#define UI_SOLAR_DIAL_CARDINAL_MARKER_OPA LV_OPA_50
#define UI_SOLAR_DIAL_CARDINAL_MARKER_RADIUS_PX 96
#define UI_FACE_TIME_Y_PX                 158
#define UI_FACE_DATE_Y_PX                 26
#define UI_FACE_DATE_RIGHT_OFFSET_PX      24
#define UI_FACE_DAY_TOP_OFFSET_PX         42
#define UI_FACE_OUTER_RING_SIZE_PX        (UI_SOLAR_DIAL_OUTER_RING_RADIUS_PX * 2)
#define UI_FACE_OUTER_RING_Y_PX           (UI_SOLAR_DIAL_CENTER_Y_PX - UI_SOLAR_DIAL_OUTER_RING_RADIUS_PX)
#define UI_FACE_OUTER_RING_WIDTH_PX       UI_SOLAR_DIAL_OUTER_RING_WIDTH_PX
#define UI_FACE_INNER_RING_SIZE_PX        (UI_SOLAR_DIAL_INNER_RING_RADIUS_PX * 2)
#define UI_FACE_INNER_RING_Y_PX           (UI_SOLAR_DIAL_CENTER_Y_PX - UI_SOLAR_DIAL_INNER_RING_RADIUS_PX)
#define UI_FACE_INNER_RING_WIDTH_PX       UI_SOLAR_DIAL_INNER_RING_WIDTH_PX
#define UI_FACE_TIME_RING_SIZE_PX         (UI_RING_TIME_RADIUS_PX * 2)
#define UI_FACE_TIME_RING_Y_PX            (UI_SOLAR_DIAL_CENTER_Y_PX - UI_RING_TIME_RADIUS_PX)
#define UI_FACE_TIME_RING_WIDTH_PX        UI_RING_TIME_WIDTH_PX
#define UI_FACE_SOLAR_MARKER_SIZE_PX      UI_SOLAR_DIAL_SUN_DIAMETER_PX
#define UI_FACE_INFO_CARD_WIDTH_PX        196
#define UI_FACE_INFO_CARD_HEIGHT_PX       50
#define UI_FACE_INFO_CARD_Y_PX            350
#define UI_FACE_COMPLICATION_WIDTH_PX     104
#define UI_FACE_COMPLICATION_HEIGHT_PX    24
#define UI_FACE_COMPLICATION_SIZE_PX      56
#define UI_FACE_COMPLICATION_Y_PX         74
#define UI_FACE_SOLAR_RADIUS_PX           UI_SOLAR_DIAL_INNER_RING_RADIUS_PX
#define UI_FACE_COMPLICATION_TOP_Y_PX     42
#define UI_FACE_COMPLICATION_BOTTOM_Y_PX  62
#define UI_FACE_COMPLICATION_OFFSET_X_PX  24
#define UI_FACE_COMPLICATION_ICON_SIZE_PX 10
#define UI_FACE_BOTTOM_CENTER_Y_PX        324
#define UI_FACE_MORNING_START_HOUR        5U
#define UI_FACE_DAY_START_HOUR            7U
#define UI_FACE_EVENING_START_HOUR        18U
#define UI_FACE_NIGHT_START_HOUR          20U
#define UI_COLOR_FACE_DAWN_HEX            0xFF9F0AUL
#define UI_COLOR_FACE_DAY_SKY_HEX         0x5AC8FAUL
#define UI_COLOR_FACE_NIGHT_SKY_HEX       0x0A1630UL
#define UI_COLOR_FACE_DUSK_HEX            0xFF6B35UL
#define UI_COLOR_FACE_ORBIT_TRACK_HEX     0x2A2A2DUL
#define UI_COLOR_FACE_CARD_HEX            0x141416UL
#define UI_COLOR_FACE_CARD_ALT_HEX        0x1E1E22UL
#define UI_COLOR_FACE_WARM_TEXT_HEX       0xFFD27DUL
#define UI_COLOR_FACE_COOL_TEXT_HEX       0x8FD3FFUL

#define UI_COLOR_FACE_DAWN                lv_color_hex(UI_COLOR_FACE_DAWN_HEX)
#define UI_COLOR_FACE_DAY_SKY             lv_color_hex(UI_COLOR_FACE_DAY_SKY_HEX)
#define UI_COLOR_FACE_NIGHT_SKY           lv_color_hex(UI_COLOR_FACE_NIGHT_SKY_HEX)
#define UI_COLOR_FACE_DUSK                lv_color_hex(UI_COLOR_FACE_DUSK_HEX)
#define UI_COLOR_FACE_ORBIT_TRACK         lv_color_hex(UI_COLOR_FACE_ORBIT_TRACK_HEX)
#define UI_COLOR_FACE_CARD                lv_color_hex(UI_COLOR_FACE_CARD_HEX)
#define UI_COLOR_FACE_CARD_ALT            lv_color_hex(UI_COLOR_FACE_CARD_ALT_HEX)
#define UI_COLOR_FACE_WARM_TEXT           lv_color_hex(UI_COLOR_FACE_WARM_TEXT_HEX)
#define UI_COLOR_FACE_COOL_TEXT           lv_color_hex(UI_COLOR_FACE_COOL_TEXT_HEX)
#define UI_APP_GRID_CLUSTER_CAPACITY      19U
#define UI_APP_GRID_HEX_X_STEP_PX         70
#define UI_APP_GRID_HEX_Y_STEP_PX         61
#define UI_APP_GRID_CLUSTER_PITCH_PX      262
#define UI_APP_GRID_CENTER_X_PX           (UI_SCREEN_WIDTH / 2)
#define UI_APP_GRID_CENTER_Y_PX           224
#define UI_APP_GRID_TOP_OFFSET_PX         0
#define UI_APP_GRID_BOTTOM_PADDING_PX     72
#define UI_APP_ICON_SIZE_FOCUS_PX         76
#define UI_APP_ICON_SIZE_MID_PX           62
#define UI_APP_ICON_SIZE_EDGE_PX          54
#define UI_APP_ICON_GLYPH_CHARS           2U
#define UI_APP_GRID_FOCUS_RADIUS_PX       150
#define UI_APP_GRID_SIDE_SHRINK_PX        6
#define UI_APP_GRID_LABEL_FADE_RADIUS_PX  104
#define UI_APP_GRID_CENTER_PULL_PX        0
#define UI_HEADER_TOP_OFFSET_PX           22
#define UI_HEADER_SIDE_PADDING_PX         20
#define UI_CARD_HEIGHT_PX                 88
#define UI_SETTING_ROW_HEIGHT_PX          56
#define UI_SETTING_VALUE_WIDTH_PX         86
#define UI_SLIDER_WIDTH_PX                170
#define UI_MAX_REGISTERED_APPS            24U

/* Allow short wrist-scale swipes and natural diagonal drift. */
#define UI_GESTURE_MIN_DISTANCE_PX        32
#define UI_GESTURE_MAX_CROSS_AXIS_PX      72
#define UI_GESTURE_MAX_DURATION_MS        900U
#define UI_GESTURE_EDGE_ZONE_PX           48

#define UI_SCREEN_TRANSITION_MS           180U
#define UI_SCREEN_TRANSITION_DELAY_MS     0U
#define UI_CLOCK_UPDATE_PERIOD_MS         WATCH_OS_FACE_REFRESH_PERIOD_MS
#define UI_LVGL_TASK_PERIOD_MS            5U
#define UI_LVGL_TASK_STACK_SIZE           8192U
#define UI_LVGL_TASK_PRIORITY             5U
#define UI_ANIMATION_PATH_CALLBACK        lv_anim_path_ease_in_out

/** Screens owned by the UI manager. */
typedef enum {
    UI_SCREEN_FACE = 0,
    UI_SCREEN_GRID,
    UI_SCREEN_CONTROL,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_APP,
    UI_SCREEN_COUNT
} ui_screen_id_t;

/** High-level swipe directions emitted by the gesture engine. */
typedef enum {
    UI_SWIPE_NONE = 0,
    UI_SWIPE_UP,
    UI_SWIPE_DOWN,
    UI_SWIPE_LEFT,
    UI_SWIPE_RIGHT
} ui_swipe_direction_t;

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_UI_DEFS_H */