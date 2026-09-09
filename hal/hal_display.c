/**
 * @file hal_display.c
 * @brief Board display bridge for LVGL 8.
 */

#include "hal/hal_display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "hal/watch_board.h"
#include "ui/ui_defs.h"

#define HAL_DISPLAY_BUFFER_CAPS (MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT)

typedef struct {
    bool initialized;
    bool dimmed;
    bool sleeping;
    uint8_t brightness_percent;
    lv_disp_t *lvgl_display;
    lv_disp_drv_t lvgl_driver;
    lv_disp_draw_buf_t lvgl_draw_buffer;
    lv_color_t *draw_buffer_primary;
    lv_color_t *draw_buffer_secondary;
    size_t draw_buffer_pixels;
    esp_lcd_panel_io_handle_t panel_io_handle;
    esp_lcd_panel_handle_t panel_handle;
} hal_display_state_t;

static const char *TAG = "hal_display";
static hal_display_state_t s_display_state;

static esp_err_t hal_display_alloc_draw_buffers(void);
static void hal_display_free_draw_buffers(void);
static esp_err_t hal_display_apply_brightness(uint8_t brightness_percent);
static void hal_display_flush_callback(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_buffer);
static bool hal_display_flush_done_callback(esp_lcd_panel_io_handle_t panel_io,
                                            esp_lcd_panel_io_event_data_t *event_data,
                                            void *user_context);
static void hal_display_rounder_callback(lv_disp_drv_t *disp_drv, lv_area_t *area);

esp_err_t hal_display_init(void)
{
    esp_err_t ret = ESP_OK;

    if(s_display_state.initialized) {
        return ESP_OK;
    }

    memset(&s_display_state, 0, sizeof(s_display_state));
    s_display_state.brightness_percent = WATCH_OS_ACTIVE_BRIGHTNESS_PERCENT;

    const size_t max_transfer_bytes = UI_SCREEN_WIDTH * UI_DISPLAY_DRAW_BUFFER_LINES * sizeof(lv_color_t);
    ESP_GOTO_ON_ERROR(watch_board_display_new(max_transfer_bytes,
                                              &s_display_state.panel_handle,
                                              &s_display_state.panel_io_handle),
                      cleanup,
                      TAG,
                      "board display initialization failed");
    ESP_GOTO_ON_ERROR(hal_display_alloc_draw_buffers(), cleanup, TAG, "draw buffer allocation failed");

    lv_disp_draw_buf_init(&s_display_state.lvgl_draw_buffer,
                          s_display_state.draw_buffer_primary,
                          s_display_state.draw_buffer_secondary,
                          (uint32_t)s_display_state.draw_buffer_pixels);

    lv_disp_drv_init(&s_display_state.lvgl_driver);
    s_display_state.lvgl_driver.hor_res = UI_SCREEN_WIDTH;
    s_display_state.lvgl_driver.ver_res = UI_SCREEN_HEIGHT;
    s_display_state.lvgl_driver.flush_cb = hal_display_flush_callback;
    s_display_state.lvgl_driver.rounder_cb = hal_display_rounder_callback;
    s_display_state.lvgl_driver.draw_buf = &s_display_state.lvgl_draw_buffer;
    s_display_state.lvgl_driver.user_data = &s_display_state;

    const esp_lcd_panel_io_callbacks_t io_callbacks = {
        .on_color_trans_done = hal_display_flush_done_callback,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(s_display_state.panel_io_handle,
                                                                &io_callbacks,
                                                                &s_display_state.lvgl_driver),
                      cleanup,
                      TAG,
                      "panel callback registration failed");

    s_display_state.lvgl_display = lv_disp_drv_register(&s_display_state.lvgl_driver);
    if(s_display_state.lvgl_display == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ESP_GOTO_ON_ERROR(hal_display_apply_brightness(s_display_state.brightness_percent),
                      cleanup,
                      TAG,
                      "initial brightness failed");

    s_display_state.initialized = true;
    ESP_LOGI(TAG, "board display ready at %ux%u", UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT);
    return ESP_OK;

cleanup:
    hal_display_deinit();
    return ret;
}

void hal_display_deinit(void)
{
    if(s_display_state.lvgl_display != NULL) {
        lv_disp_remove(s_display_state.lvgl_display);
        s_display_state.lvgl_display = NULL;
    }

    if(s_display_state.panel_handle != NULL) {
        (void)esp_lcd_panel_disp_on_off(s_display_state.panel_handle, false);
        (void)esp_lcd_panel_del(s_display_state.panel_handle);
        s_display_state.panel_handle = NULL;
    }

    if(s_display_state.panel_io_handle != NULL) {
        (void)esp_lcd_panel_io_del(s_display_state.panel_io_handle);
        s_display_state.panel_io_handle = NULL;
    }

    (void)watch_board_display_bus_deinit();
    hal_display_free_draw_buffers();
    memset(&s_display_state, 0, sizeof(s_display_state));
}

esp_err_t hal_display_set_brightness(uint8_t brightness_percent)
{
    esp_err_t ret;

    if(brightness_percent > 100U) {
        brightness_percent = 100U;
    }

    if(!s_display_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if(brightness_percent == s_display_state.brightness_percent) {
        return ESP_OK;
    }

    if(s_display_state.sleeping || s_display_state.dimmed) {
        s_display_state.brightness_percent = brightness_percent;
        return ESP_OK;
    }

    ret = hal_display_apply_brightness(brightness_percent);
    if(ret == ESP_OK) {
        s_display_state.brightness_percent = brightness_percent;
    }
    return ret;
}

uint8_t hal_display_get_brightness(void)
{
    return s_display_state.brightness_percent;
}

esp_err_t hal_display_set_dimmed(bool dimmed)
{
    s_display_state.dimmed = dimmed;
    if(s_display_state.sleeping) {
        return ESP_OK;
    }

    return hal_display_apply_brightness(dimmed ? WATCH_OS_IDLE_DIM_BRIGHTNESS_PERCENT :
                                                  s_display_state.brightness_percent);
}

esp_err_t hal_display_set_sleeping(bool sleeping)
{
    if(!s_display_state.initialized || s_display_state.panel_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if(sleeping == s_display_state.sleeping) {
        return ESP_OK;
    }

    if(sleeping) {
        ESP_RETURN_ON_ERROR(hal_display_apply_brightness(0U), TAG, "display off failed");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_display_state.panel_handle, false), TAG, "panel off failed");
        s_display_state.sleeping = true;
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_display_state.panel_handle, true), TAG, "panel on failed");
    s_display_state.sleeping = false;
    return hal_display_apply_brightness(s_display_state.dimmed ? WATCH_OS_IDLE_DIM_BRIGHTNESS_PERCENT :
                                                                  s_display_state.brightness_percent);
}

bool hal_display_is_sleeping(void)
{
    return s_display_state.sleeping;
}

lv_disp_t *hal_display_get_lvgl_display(void)
{
    return s_display_state.lvgl_display;
}

static esp_err_t hal_display_alloc_draw_buffers(void)
{
    const size_t pixel_count = (size_t)UI_SCREEN_WIDTH * (size_t)UI_DISPLAY_DRAW_BUFFER_LINES;
    const size_t buffer_size_bytes = pixel_count * sizeof(lv_color_t);

    s_display_state.draw_buffer_primary = heap_caps_malloc(buffer_size_bytes, HAL_DISPLAY_BUFFER_CAPS);
    s_display_state.draw_buffer_secondary = heap_caps_malloc(buffer_size_bytes, HAL_DISPLAY_BUFFER_CAPS);
    if(s_display_state.draw_buffer_primary == NULL || s_display_state.draw_buffer_secondary == NULL) {
        hal_display_free_draw_buffers();
        return ESP_ERR_NO_MEM;
    }

    s_display_state.draw_buffer_pixels = pixel_count;
    return ESP_OK;
}

static void hal_display_free_draw_buffers(void)
{
    heap_caps_free(s_display_state.draw_buffer_primary);
    heap_caps_free(s_display_state.draw_buffer_secondary);
    s_display_state.draw_buffer_primary = NULL;
    s_display_state.draw_buffer_secondary = NULL;
    s_display_state.draw_buffer_pixels = 0U;
}

static esp_err_t hal_display_apply_brightness(uint8_t brightness_percent)
{
    if(s_display_state.panel_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return watch_board_display_set_brightness(s_display_state.panel_handle, brightness_percent);
}

static void hal_display_flush_callback(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_buffer)
{
    hal_display_state_t *display_state = (hal_display_state_t *)disp_drv->user_data;

    const esp_err_t ret = esp_lcd_panel_draw_bitmap(display_state->panel_handle,
                                                    area->x1,
                                                    area->y1,
                                                    area->x2 + 1,
                                                    area->y2 + 1,
                                                    color_buffer);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "panel draw failed: %s", esp_err_to_name(ret));
        lv_disp_flush_ready(disp_drv);
    }
}

static bool hal_display_flush_done_callback(esp_lcd_panel_io_handle_t panel_io,
                                            esp_lcd_panel_io_event_data_t *event_data,
                                            void *user_context)
{
    (void)panel_io;
    (void)event_data;
    lv_disp_flush_ready((lv_disp_drv_t *)user_context);
    return false;
}

static void hal_display_rounder_callback(lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    (void)disp_drv;
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
}