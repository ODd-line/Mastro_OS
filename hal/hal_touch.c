/**
 * @file hal_touch.c
 * @brief Waveshare CST820-compatible touch bridge.
 */

#include "hal/hal_touch.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bsp/touch.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "ui/ui_defs.h"

typedef struct {
    bool initialized;
    esp_lcd_touch_handle_t touch_handle;
} hal_touch_context_t;

static const char *TAG = "hal_touch";
static hal_touch_context_t s_touch_context;

esp_err_t hal_touch_init(void)
{
    if(s_touch_context.initialized) {
        return ESP_OK;
    }

    memset(&s_touch_context, 0, sizeof(s_touch_context));
    const esp_err_t ret = bsp_touch_new(NULL, &s_touch_context.touch_handle);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Waveshare BSP touch initialization failed: %s", esp_err_to_name(ret));
        hal_touch_deinit();
        return ret;
    }

    s_touch_context.initialized = true;
    ESP_LOGI(TAG, "CST820-compatible touch ready");
    return ESP_OK;
}

esp_err_t hal_touch_read(hal_touch_state_t *touch_state)
{
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t strength = 0;
    uint8_t touch_count = 0;

    if(touch_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(touch_state, 0, sizeof(*touch_state));
    if(!s_touch_context.initialized || s_touch_context.touch_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t ret = esp_lcd_touch_read_data(s_touch_context.touch_handle);
    if(ret != ESP_OK) {
        return ret;
    }

    touch_state->touched = esp_lcd_touch_get_coordinates(s_touch_context.touch_handle,
                                                         &x,
                                                         &y,
                                                         &strength,
                                                         &touch_count,
                                                         1U);
    touch_state->touch_count = touch_count;
    if(!touch_state->touched) {
        return ESP_OK;
    }

    touch_state->x = (x < UI_SCREEN_WIDTH) ? x : (uint16_t)(UI_SCREEN_WIDTH - 1U);
    touch_state->y = (y < UI_SCREEN_HEIGHT) ? y : (uint16_t)(UI_SCREEN_HEIGHT - 1U);
    return ESP_OK;
}

void hal_touch_deinit(void)
{
    if(s_touch_context.touch_handle != NULL) {
        (void)esp_lcd_touch_del(s_touch_context.touch_handle);
    }

    memset(&s_touch_context, 0, sizeof(s_touch_context));
}