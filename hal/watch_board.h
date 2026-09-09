#ifndef WATCH_BOARD_H
#define WATCH_BOARD_H

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

typedef struct {
    const char *id;
    const char *name;
    uint32_t minimum_flash_bytes;
    size_t minimum_psram_bytes;
} watch_board_profile_t;

const watch_board_profile_t *watch_board_get_profile(void);
esp_err_t watch_board_release_peripherals(void);
esp_err_t watch_board_display_new(size_t max_transfer_bytes,
                                  esp_lcd_panel_handle_t *panel_handle,
                                  esp_lcd_panel_io_handle_t *panel_io_handle);
esp_err_t watch_board_display_set_brightness(esp_lcd_panel_handle_t panel_handle, uint8_t brightness_percent);
esp_err_t watch_board_display_bus_deinit(void);
esp_err_t watch_board_touch_new(esp_lcd_touch_handle_t *touch_handle);
i2c_master_bus_handle_t watch_board_i2c_get_handle(void);
uint32_t watch_board_i2c_clock_hz(void);

#endif