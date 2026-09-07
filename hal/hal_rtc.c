/**
 * @file hal_rtc.c
 * @brief PCF85063 real-time clock access over the board I2C bus.
 */

#include "hal/hal_rtc.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "hal/watch_board.h"

#define HAL_RTC_I2C_ADDRESS       0x51U
#define HAL_RTC_FIRST_TIME_REG    0x04U
#define HAL_RTC_TIME_REG_COUNT    7U
#define HAL_RTC_I2C_TIMEOUT_MS    100
#define HAL_RTC_MIN_YEAR          2024
#define HAL_RTC_MAX_YEAR          2099

typedef struct {
    bool initialized;
    i2c_master_dev_handle_t device;
} hal_rtc_state_t;

static const char *TAG = "hal_rtc";
static hal_rtc_state_t s_rtc_state;

static uint8_t hal_rtc_bcd_to_binary(uint8_t value);
static uint8_t hal_rtc_binary_to_bcd(uint8_t value);

esp_err_t hal_rtc_init(void)
{
    if(s_rtc_state.initialized) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus = watch_board_i2c_get_handle();
    if(bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, HAL_RTC_I2C_ADDRESS, HAL_RTC_I2C_TIMEOUT_MS),
                        TAG,
                        "PCF85063 not found");

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = HAL_RTC_I2C_ADDRESS,
        .scl_speed_hz = watch_board_i2c_clock_hz(),
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device_config, &s_rtc_state.device),
                        TAG,
                        "failed to attach PCF85063");

    s_rtc_state.initialized = true;
    ESP_LOGI(TAG, "PCF85063 RTC ready");
    return ESP_OK;
}

esp_err_t hal_rtc_get_time(struct tm *time_out)
{
    uint8_t register_address = HAL_RTC_FIRST_TIME_REG;
    uint8_t registers[HAL_RTC_TIME_REG_COUNT];

    if(time_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if(!s_rtc_state.initialized || s_rtc_state.device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(s_rtc_state.device,
                                                    &register_address,
                                                    sizeof(register_address),
                                                    registers,
                                                    sizeof(registers),
                                                    HAL_RTC_I2C_TIMEOUT_MS),
                        TAG,
                        "failed to read PCF85063 time");

    if((registers[0] & 0x80U) != 0U) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(time_out, 0, sizeof(*time_out));
    time_out->tm_sec = hal_rtc_bcd_to_binary(registers[0] & 0x7FU);
    time_out->tm_min = hal_rtc_bcd_to_binary(registers[1] & 0x7FU);
    time_out->tm_hour = hal_rtc_bcd_to_binary(registers[2] & 0x3FU);
    time_out->tm_mday = hal_rtc_bcd_to_binary(registers[3] & 0x3FU);
    time_out->tm_wday = hal_rtc_bcd_to_binary(registers[4] & 0x07U);
    time_out->tm_mon = (int)hal_rtc_bcd_to_binary(registers[5] & 0x1FU) - 1;
    time_out->tm_year = (int)hal_rtc_bcd_to_binary(registers[6]) + 100;
    time_out->tm_isdst = -1;

    const int year = time_out->tm_year + 1900;
    if(year < HAL_RTC_MIN_YEAR || year > HAL_RTC_MAX_YEAR ||
       time_out->tm_mon < 0 || time_out->tm_mon > 11 ||
       time_out->tm_mday < 1 || time_out->tm_mday > 31 ||
       time_out->tm_hour > 23 || time_out->tm_min > 59 || time_out->tm_sec > 59) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

esp_err_t hal_rtc_set_time(const struct tm *time_value)
{
    uint8_t payload[HAL_RTC_TIME_REG_COUNT + 1U];
    const int year = time_value != NULL ? time_value->tm_year + 1900 : 0;

    if(time_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if(!s_rtc_state.initialized || s_rtc_state.device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if(year < HAL_RTC_MIN_YEAR || year > HAL_RTC_MAX_YEAR ||
       time_value->tm_mon < 0 || time_value->tm_mon > 11 ||
       time_value->tm_mday < 1 || time_value->tm_mday > 31 ||
       time_value->tm_wday < 0 || time_value->tm_wday > 6 ||
       time_value->tm_hour < 0 || time_value->tm_hour > 23 ||
       time_value->tm_min < 0 || time_value->tm_min > 59 ||
       time_value->tm_sec < 0 || time_value->tm_sec > 59) {
        return ESP_ERR_INVALID_ARG;
    }

    payload[0] = HAL_RTC_FIRST_TIME_REG;
    payload[1] = hal_rtc_binary_to_bcd((uint8_t)time_value->tm_sec);
    payload[2] = hal_rtc_binary_to_bcd((uint8_t)time_value->tm_min);
    payload[3] = hal_rtc_binary_to_bcd((uint8_t)time_value->tm_hour);
    payload[4] = hal_rtc_binary_to_bcd((uint8_t)time_value->tm_mday);
    payload[5] = hal_rtc_binary_to_bcd((uint8_t)time_value->tm_wday);
    payload[6] = hal_rtc_binary_to_bcd((uint8_t)(time_value->tm_mon + 1));
    payload[7] = hal_rtc_binary_to_bcd((uint8_t)(year - 2000));

    return i2c_master_transmit(s_rtc_state.device, payload, sizeof(payload), HAL_RTC_I2C_TIMEOUT_MS);
}

static uint8_t hal_rtc_bcd_to_binary(uint8_t value)
{
    return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint8_t hal_rtc_binary_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}
