/**
 * @file wifi_manager.c
 * @brief Station-mode Wi-Fi management and network-connected sync triggers.
 */

#include "utils/wifi_manager.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "utils/time_utils.h"
#include "utils/weather_service.h"

typedef struct {
    bool initialized;
    bool stack_started;
    bool radio_started;
    bool enabled;
    bool configured;
    bool connected;
    uint8_t retry_count;
    wifi_manager_state_t state;
    char ssid[33];
    char password[65];
    esp_netif_t *station_netif;
} wifi_manager_context_t;

static const char *TAG = "wifi_manager";
static wifi_manager_context_t s_wifi_manager;

static void wifi_manager_event_handler(void *arg,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void *event_data);
static bool wifi_manager_has_credentials(void);
static esp_err_t wifi_manager_load_credentials(void);
static esp_err_t wifi_manager_store_credentials(const char *ssid, const char *password);
static esp_err_t wifi_manager_start_stack(void);
static esp_err_t wifi_manager_apply_station_config(void);

esp_err_t wifi_manager_init(void)
{
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret;

    if(s_wifi_manager.initialized) {
        return ESP_OK;
    }

    memset(&s_wifi_manager, 0, sizeof(s_wifi_manager));
    s_wifi_manager.state = WIFI_MANAGER_STATE_DISABLED;
    snprintf(s_wifi_manager.ssid, sizeof(s_wifi_manager.ssid), "%s", WATCH_OS_WIFI_SSID);
    snprintf(s_wifi_manager.password, sizeof(s_wifi_manager.password), "%s", WATCH_OS_WIFI_PASSWORD);
    ESP_RETURN_ON_ERROR(wifi_manager_load_credentials(), TAG, "failed to load stored wifi credentials");
    s_wifi_manager.configured = wifi_manager_has_credentials();
    s_wifi_manager.enabled = s_wifi_manager.configured;
    if(!s_wifi_manager.configured) {
        s_wifi_manager.initialized = true;
        ESP_LOGW(TAG, "WATCH_OS_WIFI_SSID is empty; Wi-Fi weather sync is disabled");
        return ESP_OK;
    }

    (void)wifi_init_config;
    ret = wifi_manager_start_stack();
    if(ret != ESP_OK) {
        return ret;
    }

    ESP_RETURN_ON_ERROR(wifi_manager_apply_station_config(), TAG, "failed to apply station config");

    s_wifi_manager.initialized = true;
    return ESP_OK;
}

bool wifi_manager_is_configured(void)
{
    return s_wifi_manager.configured;
}

bool wifi_manager_is_connected(void)
{
    return s_wifi_manager.connected;
}

bool wifi_manager_is_enabled(void)
{
    return s_wifi_manager.enabled;
}

wifi_manager_state_t wifi_manager_get_state(void)
{
    return s_wifi_manager.state;
}

esp_err_t wifi_manager_get_ssid(char *buffer, size_t buffer_size)
{
    if(buffer == NULL || buffer_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if(!s_wifi_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    snprintf(buffer, buffer_size, "%s", s_wifi_manager.ssid);
    return ESP_OK;
}

esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password)
{
    if(ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(strlen(ssid) >= sizeof(s_wifi_manager.ssid) || strlen(password) >= sizeof(s_wifi_manager.password)) {
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_RETURN_ON_ERROR(wifi_manager_store_credentials(ssid, password), TAG, "failed to store wifi credentials");

    snprintf(s_wifi_manager.ssid, sizeof(s_wifi_manager.ssid), "%s", ssid);
    snprintf(s_wifi_manager.password, sizeof(s_wifi_manager.password), "%s", password);
    s_wifi_manager.configured = wifi_manager_has_credentials();
    if(!s_wifi_manager.configured && s_wifi_manager.radio_started) {
        (void)esp_wifi_disconnect();
    }
    if(!s_wifi_manager.initialized || !s_wifi_manager.configured) {
        s_wifi_manager.state = s_wifi_manager.configured ? WIFI_MANAGER_STATE_DISCONNECTED : WIFI_MANAGER_STATE_DISABLED;
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(wifi_manager_start_stack(), TAG, "failed to start wifi stack");
    return wifi_manager_apply_station_config();
}

esp_err_t wifi_manager_reconnect(void)
{
    if(!s_wifi_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if(!s_wifi_manager.configured) {
        return ESP_ERR_INVALID_STATE;
    }

    s_wifi_manager.enabled = true;
    s_wifi_manager.retry_count = 0U;
    ESP_RETURN_ON_ERROR(wifi_manager_start_stack(), TAG, "failed to start wifi stack");
    return wifi_manager_apply_station_config();
}

esp_err_t wifi_manager_set_enabled(bool enabled)
{
    if(!s_wifi_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if(enabled && !s_wifi_manager.configured) {
        return ESP_ERR_INVALID_STATE;
    }

    s_wifi_manager.enabled = enabled;
    if(!enabled) {
        s_wifi_manager.connected = false;
        s_wifi_manager.retry_count = 0U;
        s_wifi_manager.state = WIFI_MANAGER_STATE_DISABLED;
        if(s_wifi_manager.radio_started) {
            ESP_RETURN_ON_ERROR(esp_wifi_stop(), TAG, "failed to stop wifi");
            s_wifi_manager.radio_started = false;
        }
        return ESP_OK;
    }

    return wifi_manager_reconnect();
}

static void wifi_manager_event_handler(void *arg,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void *event_data)
{
    (void)arg;
    (void)event_data;

    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_wifi_manager.state = WIFI_MANAGER_STATE_CONNECTING;
        (void)esp_wifi_connect();
        return;
    }

    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_manager.connected = false;
        s_wifi_manager.state = s_wifi_manager.enabled ? WIFI_MANAGER_STATE_DISCONNECTED : WIFI_MANAGER_STATE_DISABLED;
        if(!s_wifi_manager.enabled) {
            return;
        }
        if(s_wifi_manager.retry_count < WATCH_OS_WIFI_MAXIMUM_RETRY) {
            s_wifi_manager.retry_count++;
            s_wifi_manager.state = WIFI_MANAGER_STATE_CONNECTING;
            (void)esp_wifi_connect();
        }
        return;
    }

    if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_manager.connected = true;
        s_wifi_manager.retry_count = 0;
        s_wifi_manager.state = WIFI_MANAGER_STATE_CONNECTED;
        (void)time_utils_force_sync(false, 0U);
        (void)weather_service_request_refresh();
    }
}

static bool wifi_manager_has_credentials(void)
{
    return (s_wifi_manager.ssid[0] != '\0');
}

static esp_err_t wifi_manager_load_credentials(void)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;
    size_t ssid_size = sizeof(s_wifi_manager.ssid);
    size_t password_size = sizeof(s_wifi_manager.password);

    ret = nvs_open(WATCH_OS_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if(ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if(ret != ESP_OK) {
        return ret;
    }

    ret = nvs_get_str(nvs_handle, WATCH_OS_NVS_KEY_WIFI_SSID, s_wifi_manager.ssid, &ssid_size);
    if(ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_get_str(nvs_handle, WATCH_OS_NVS_KEY_WIFI_PASSWORD, s_wifi_manager.password, &password_size);
    if(ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs_handle);
        return ret;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

static esp_err_t wifi_manager_store_credentials(const char *ssid, const char *password)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;

    ESP_RETURN_ON_ERROR(nvs_open(WATCH_OS_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle),
                        TAG,
                        "nvs_open failed for wifi credentials");

    ret = nvs_set_str(nvs_handle, WATCH_OS_NVS_KEY_WIFI_SSID, ssid);
    if(ret == ESP_OK) {
        ret = nvs_set_str(nvs_handle, WATCH_OS_NVS_KEY_WIFI_PASSWORD, password);
    }
    if(ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return ret;
}

static esp_err_t wifi_manager_start_stack(void)
{
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret;

    if(s_wifi_manager.stack_started) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "failed to init netif");
    ret = esp_event_loop_create_default();
    if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    if(s_wifi_manager.station_netif == NULL) {
        s_wifi_manager.station_netif = esp_netif_create_default_wifi_sta();
        if(s_wifi_manager.station_netif == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_init_config), TAG, "failed to init wifi");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT,
                                                   ESP_EVENT_ANY_ID,
                                                   &wifi_manager_event_handler,
                                                   NULL),
                        TAG,
                        "failed to register wifi event handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT,
                                                   IP_EVENT_STA_GOT_IP,
                                                   &wifi_manager_event_handler,
                                                   NULL),
                        TAG,
                        "failed to register ip event handler");

    s_wifi_manager.stack_started = true;
    return ESP_OK;
}

static esp_err_t wifi_manager_apply_station_config(void)
{
    wifi_config_t wifi_config;
    esp_err_t ret;
    const size_t ssid_length = strnlen(s_wifi_manager.ssid, sizeof(wifi_config.sta.ssid));
    const size_t password_length = strnlen(s_wifi_manager.password, sizeof(wifi_config.sta.password));

    if(!s_wifi_manager.stack_started) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, s_wifi_manager.ssid, ssid_length);
    memcpy(wifi_config.sta.password, s_wifi_manager.password, password_length);
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.failure_retry_cnt = 0;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "failed to set station mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "failed to set wifi config");

    if(!s_wifi_manager.radio_started) {
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "failed to start wifi");
        s_wifi_manager.radio_started = true;
    }
    else {
        ret = esp_wifi_disconnect();
        if(ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_CONNECT) {
            return ret;
        }

        ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "failed to reconnect wifi");
    }

    s_wifi_manager.retry_count = 0U;
    s_wifi_manager.state = WIFI_MANAGER_STATE_CONNECTING;
    return ESP_OK;
}