/**
 * @file wifi_manager.h
 * @brief Station-mode Wi-Fi lifecycle helpers for automatic cloud-backed sync.
 */

#ifndef WATCH_OS_WIFI_MANAGER_H
#define WATCH_OS_WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_MANAGER_STATE_DISABLED = 0,
    WIFI_MANAGER_STATE_DISCONNECTED,
    WIFI_MANAGER_STATE_CONNECTING,
    WIFI_MANAGER_STATE_CONNECTED
} wifi_manager_state_t;

esp_err_t wifi_manager_init(void);
bool wifi_manager_is_configured(void);
bool wifi_manager_is_enabled(void);
bool wifi_manager_is_connected(void);
wifi_manager_state_t wifi_manager_get_state(void);
esp_err_t wifi_manager_get_ssid(char *buffer, size_t buffer_size);
esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password);
esp_err_t wifi_manager_forget_credentials(void);
esp_err_t wifi_manager_set_enabled(bool enabled);
esp_err_t wifi_manager_reconnect(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_OS_WIFI_MANAGER_H */