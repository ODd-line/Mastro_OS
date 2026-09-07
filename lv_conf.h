/**
 * @file lv_conf.h
 * @brief LVGL 8.3 configuration for an ESP32-S3 smartwatch with PSRAM.
 */

#ifndef WATCH_OS_LV_CONF_H
#define WATCH_OS_LV_CONF_H

#include <stdint.h>

/* Color settings for RGB565 SPI panels. */
#define LV_COLOR_DEPTH                   16
#define LV_COLOR_16_SWAP                 1
#define LV_COLOR_CHROMA_KEY              lv_color_hex(0x00FF00)

/* Keep general LVGL allocations in byte-addressable PSRAM. The display HAL
 * allocates its transfer buffers separately with DMA-capable capabilities. */
#define LV_MEM_CUSTOM                    1
#define LV_MEM_CUSTOM_INCLUDE            "esp_heap_caps.h"
#define LV_MEM_CUSTOM_ALLOC(size)        heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_MEM_CUSTOM_FREE               heap_caps_free
#define LV_MEM_CUSTOM_REALLOC(ptr, size) heap_caps_realloc((ptr), (size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_MEM_BUF_MAX_NUM              16
#define LV_MEMCPY_MEMSET_STD            1

/* A single GUI task calls LVGL; ESP-IDF synchronization remains external. */
#define LV_ENABLE_GC                     0

/* Rendering quality balanced for a small AMOLED panel. */
#define LV_DISP_DEF_REFR_PERIOD          16
#define LV_INDEV_DEF_READ_PERIOD         10
#define LV_TICK_CUSTOM                   1
#define LV_TICK_CUSTOM_INCLUDE           "esp_timer.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR     ((uint32_t)(esp_timer_get_time() / 1000ULL))
#define LV_DPI_DEF                       220
#define LV_DRAW_COMPLEX                  1
#define LV_SHADOW_CACHE_SIZE             0
#define LV_CIRCLE_CACHE_SIZE             8
#define LV_LAYER_SIMPLE_BUF_SIZE         (24U * 1024U)
#define LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE (3U * 1024U)
#define LV_IMG_CACHE_DEF_SIZE            0
#define LV_GRADIENT_MAX_STOPS            2
#define LV_GRAD_CACHE_DEF_SIZE           0
#define LV_DITHER_GRADIENT               0
#define LV_DISP_ROT_MAX_BUF              (10U * 1024U)

/* Logging can be enabled per build without editing this file. */
#ifndef LV_USE_LOG
#define LV_USE_LOG                       0
#endif
#define LV_LOG_LEVEL                     LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF                    1

/* Assertions catch integration defects early in development builds. */
#ifndef NDEBUG
#define LV_USE_ASSERT_NULL               1
#define LV_USE_ASSERT_MALLOC             1
#define LV_USE_ASSERT_STYLE              1
#define LV_USE_ASSERT_MEM_INTEGRITY      0
#define LV_USE_ASSERT_OBJ                1
#else
#define LV_USE_ASSERT_NULL               0
#define LV_USE_ASSERT_MALLOC             0
#define LV_USE_ASSERT_STYLE              0
#define LV_USE_ASSERT_MEM_INTEGRITY      0
#define LV_USE_ASSERT_OBJ                0
#endif

/* Built-in fonts required by the watch face and supporting labels. */
#define LV_FONT_MONTSERRAT_12            1
#define LV_FONT_MONTSERRAT_14            1
#define LV_FONT_MONTSERRAT_32            1
#define LV_FONT_MONTSERRAT_36            1
#define LV_FONT_MONTSERRAT_28            1
#define LV_FONT_MONTSERRAT_48            1
#define LV_FONT_DEFAULT                  &lv_font_montserrat_14
#define LV_USE_FONT_COMPRESSED           0
#define LV_USE_FONT_SUBPX                0

/* Core widgets used by the initial watch screens. */
#define LV_USE_ANIMIMG                   0
#define LV_USE_ARC                       1
#define LV_USE_BAR                       1
#define LV_USE_BTN                       1
#define LV_USE_BTNMATRIX                 1
#define LV_USE_CANVAS                    0
#define LV_USE_CHECKBOX                  0
#define LV_USE_DROPDOWN                  0
#define LV_USE_IMG                       1
#define LV_USE_LABEL                     1
#define LV_USE_LINE                      0
#define LV_USE_ROLLER                    0
#define LV_USE_SLIDER                    1
#define LV_USE_SWITCH                    1
#define LV_USE_TABLE                     0
#define LV_USE_TEXTAREA                  1

/* Flex is used for compact control layouts; grid layout is not required. */
#define LV_USE_FLEX                      1
#define LV_USE_GRID                      0

/* Disable optional packages that are not part of the smartwatch shell. */
#define LV_USE_THEME_DEFAULT             0
#define LV_USE_THEME_BASIC               0
#define LV_USE_THEME_MONO                0
#define LV_USE_CALENDAR                  0
#define LV_USE_CHART                     0
#define LV_USE_COLORWHEEL                0
#define LV_USE_IMGBTN                    0
#define LV_USE_KEYBOARD                  1
#define LV_USE_LED                       0
#define LV_USE_LIST                      0
#define LV_USE_MENU                      0
#define LV_USE_METER                     0
#define LV_USE_MSGBOX                    0
#define LV_USE_SPAN                      0
#define LV_USE_SPINBOX                   0
#define LV_USE_SPINNER                   0
#define LV_USE_TABVIEW                   0
#define LV_USE_TILEVIEW                  0
#define LV_USE_WIN                       0
#define LV_USE_FS_STDIO                  0
#define LV_USE_FS_POSIX                  0
#define LV_USE_FS_WIN32                  0
#define LV_USE_FS_FATFS                  0
#define LV_USE_FS_LITTLEFS               0
#define LV_USE_PNG                       0
#define LV_USE_BMP                       0
#define LV_USE_SJPG                      0
#define LV_USE_GIF                       0
#define LV_USE_QRCODE                    0
#define LV_USE_FREETYPE                  0
#define LV_USE_RLOTTIE                   0
#define LV_USE_SNAPSHOT                  0
#define LV_USE_MONKEY                    0
#define LV_USE_GRIDNAV                   0
#define LV_USE_FRAGMENT                  0
#define LV_USE_IMGFONT                   0
#define LV_USE_MSG                       0

/* Keep LVGL's examples and demos out of production firmware. */
#define LV_BUILD_EXAMPLES                0
#define LV_USE_DEMO_WIDGETS              0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER   0
#define LV_USE_DEMO_BENCHMARK            0
#define LV_USE_DEMO_STRESS               0
#define LV_USE_DEMO_MUSIC                0

#endif /* WATCH_OS_LV_CONF_H */