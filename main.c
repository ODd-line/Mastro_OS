/**
 * @file main.c
 * @brief Smartwatch OS entry point.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "apps/app_registry.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/hal_display.h"
#include "hal/hal_touch.h"
#include "hal/watch_board.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "ui/ui_defs.h"
#include "ui/ui_manager.h"
#include "utils/time_utils.h"
#include "utils/weather_service.h"
#include "utils/wifi_manager.h"

static const char *TAG = "watch_os";

static bool hardware_preflight(void);
static void gui_task(void *task_parameter);

void app_main(void)
{
    esp_err_t ret;
    BaseType_t task_result;

    if(!hardware_preflight()) {
        ESP_LOGE(TAG, "hardware preflight failed; firmware startup stopped");
        return;
    }

    ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(watch_board_release_peripherals());
    lv_init();
    ESP_ERROR_CHECK(time_utils_init());
    ESP_ERROR_CHECK(weather_service_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(app_registry_init());
    ESP_ERROR_CHECK(app_registry_register_builtin_apps());
    ESP_ERROR_CHECK(hal_touch_init());
    ESP_ERROR_CHECK(hal_display_init());
    ESP_ERROR_CHECK(ui_manager_init());

    task_result = xTaskCreatePinnedToCore(gui_task,
                                          "gui_task",
                                          UI_LVGL_TASK_STACK_SIZE,
                                          NULL,
                                          UI_LVGL_TASK_PRIORITY,
                                          NULL,
                                          tskNO_AFFINITY);
    if(task_result != pdPASS) {
        ESP_LOGE(TAG, "failed to create gui task");
        abort();
    }

}

static bool hardware_preflight(void)
{
    const watch_board_profile_t *board = watch_board_get_profile();
    uint32_t flash_size_bytes = 0;
    const esp_err_t flash_ret = esp_flash_get_size(NULL, &flash_size_bytes);
    const size_t psram_size_bytes = esp_psram_get_size();

    if(flash_ret != ESP_OK) {
        ESP_LOGE(TAG, "could not read flash size: %s", esp_err_to_name(flash_ret));
        return false;
    }

    ESP_LOGI(TAG,
             "hardware preflight for %s: flash=%" PRIu32 " MB, PSRAM=%u MB",
             board->name,
             flash_size_bytes / (1024U * 1024U),
             (unsigned)(psram_size_bytes / (1024U * 1024U)));

    if(flash_size_bytes < board->minimum_flash_bytes) {
        ESP_LOGE(TAG, "board profile requires more flash");
        return false;
    }

    if(!esp_psram_is_initialized() || psram_size_bytes < board->minimum_psram_bytes) {
        ESP_LOGE(TAG, "board profile requires more initialized PSRAM");
        return false;
    }

    return true;
}

static void gui_task(void *task_parameter)
{
    const bool watchdog_registered = (esp_task_wdt_add(NULL) == ESP_OK);

    (void)task_parameter;

    while(true) {
        lv_tick_inc(UI_LVGL_TASK_PERIOD_MS);
        lv_timer_handler();
        if(watchdog_registered) {
            (void)esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(UI_LVGL_TASK_PERIOD_MS));
    }
}
