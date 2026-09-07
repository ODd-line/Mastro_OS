set(WATCH_BOARD_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../hal/watch_board_waveshare_v2.c")

idf_component_register(
    SRCS
        "${CMAKE_CURRENT_LIST_DIR}/../main.c"
        "${CMAKE_CURRENT_LIST_DIR}/../apps/app_registry.c"
        "${CMAKE_CURRENT_LIST_DIR}/../astronomy/solar_calc.c"
        "${CMAKE_CURRENT_LIST_DIR}/../hal/hal_display.c"
        "${CMAKE_CURRENT_LIST_DIR}/../hal/hal_rtc.c"
        "${CMAKE_CURRENT_LIST_DIR}/../hal/hal_touch.c"
        "${WATCH_BOARD_SOURCE}"
        "${CMAKE_CURRENT_LIST_DIR}/../ui/ui_manager.c"
        "${CMAKE_CURRENT_LIST_DIR}/../ui/screens/screen_app.c"
        "${CMAKE_CURRENT_LIST_DIR}/../ui/screens/screen_face.c"
        "${CMAKE_CURRENT_LIST_DIR}/../ui/screens/screen_grid.c"
        "${CMAKE_CURRENT_LIST_DIR}/../ui/screens/screen_control.c"
        "${CMAKE_CURRENT_LIST_DIR}/../ui/screens/screen_settings.c"
        "${CMAKE_CURRENT_LIST_DIR}/../utils/time_utils.c"
        "${CMAKE_CURRENT_LIST_DIR}/../utils/weather_service.c"
        "${CMAKE_CURRENT_LIST_DIR}/../utils/wifi_manager.c"
    INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/.."
    REQUIRES
        driver
        esp32_s3_touch_amoled_1_8
        esp_event
        esp_http_client
        esp_io_expander
        esp_lcd
        esp_lcd_touch
        esp_netif
        esp_psram
        esp_wifi
        json
        lvgl__lvgl
        lwip
        mbedtls
        nvs_flash
        spi_flash
)

set(WATCH_BUILD_TIME_HEADER "${CMAKE_CURRENT_BINARY_DIR}/watch_build_time.h")
add_custom_target(watch_build_time ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DOUTPUT_FILE=${WATCH_BUILD_TIME_HEADER}"
        -P "${CMAKE_CURRENT_LIST_DIR}/generate_build_time.cmake"
    BYPRODUCTS "${WATCH_BUILD_TIME_HEADER}"
    VERBATIM
)
add_dependencies(${COMPONENT_LIB} watch_build_time)
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
set_property(
    SOURCE "${CMAKE_CURRENT_LIST_DIR}/../utils/time_utils.c"
    APPEND PROPERTY OBJECT_DEPENDS "${WATCH_BUILD_TIME_HEADER}"
)