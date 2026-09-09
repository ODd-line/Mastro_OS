#include "hal/watch_board.h"

#include "bsp/display.h"
#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "bsp/touch.h"
#include "driver/spi_common.h"
#include "esp_check.h"
#include "esp_lcd_co5300.h"
#include "esp_io_expander.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board_waveshare_v2";

static const watch_board_profile_t s_profile = {
    .id = "waveshare_amoled_1_8_v2",
    .name = "Waveshare ESP32-S3-Touch-AMOLED-1.8 V2",
    .minimum_flash_bytes = 16U * 1024U * 1024U,
    .minimum_psram_bytes = 7U * 1024U * 1024U,
};

const watch_board_profile_t *watch_board_get_profile(void)
{
    return &s_profile;
}

esp_err_t watch_board_release_peripherals(void)
{
    const uint32_t reset_mask = (1U << 0) | (1U << 1) | (1U << 2);
    esp_io_expander_handle_t io_expander = bsp_io_expander_init();

    if(io_expander == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(io_expander, reset_mask, IO_EXPANDER_OUTPUT),
                        TAG,
                        "failed to configure peripheral reset outputs");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(io_expander, reset_mask, 0),
                        TAG,
                        "failed to assert peripheral resets");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(io_expander, reset_mask, 1),
                        TAG,
                        "failed to release peripheral resets");
    vTaskDelay(pdMS_TO_TICKS(200));
    return ESP_OK;
}

esp_err_t watch_board_display_new(size_t max_transfer_bytes,
                                  esp_lcd_panel_handle_t *panel_handle,
                                  esp_lcd_panel_io_handle_t *panel_io_handle)
{
    const bsp_display_config_t display_config = {
        .max_transfer_sz = (int)max_transfer_bytes,
    };

    return bsp_display_new(&display_config, panel_handle, panel_io_handle);
}

esp_err_t watch_board_display_bus_deinit(void)
{
    return spi_bus_free(BSP_LCD_SPI_NUM);
}

esp_err_t watch_board_display_set_brightness(esp_lcd_panel_handle_t panel_handle, uint8_t brightness_percent)
{
    if(panel_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_lcd_panel_co5300_set_brightness(panel_handle, brightness_percent);
}

esp_err_t watch_board_touch_new(esp_lcd_touch_handle_t *touch_handle)
{
    return bsp_touch_new(NULL, touch_handle);
}

i2c_master_bus_handle_t watch_board_i2c_get_handle(void)
{
    return bsp_i2c_get_handle();
}

uint32_t watch_board_i2c_clock_hz(void)
{
    return CONFIG_BSP_I2C_CLK_SPEED_HZ;
}