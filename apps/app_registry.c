/**
 * @file app_registry.c
 * @brief Registry-backed app catalog for the launcher.
 */

#include "apps/app_registry.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "esp_check.h"
#include "ui/ui_defs.h"

static const watch_app_descriptor_t BUILTIN_APPS[] = {
    { "phone", "Phone", "Recent calls and favorites", UI_COLOR_ACCENT_GREEN_HEX, WATCH_APP_TARGET_SHELL, true },
    { "music", "Music", "Now playing and library", UI_COLOR_ACCENT_RED_HEX, WATCH_APP_TARGET_SHELL, true },
    { "maps", "Maps", "Nearby and recent destinations", UI_COLOR_ACCENT_BLUE_HEX, WATCH_APP_TARGET_SHELL, true },
    { "workout", "Workout", "Planning; motion sensor unavailable", UI_COLOR_ACCENT_GREEN_HEX, WATCH_APP_TARGET_SHELL, true },
    { "heart", "Heart", "Heart-rate sensor unavailable", UI_COLOR_ACCENT_RED_HEX, WATCH_APP_TARGET_SHELL, true },
    { "home", "Home", "Rooms, scenes, and accessories", UI_COLOR_ACCENT_GREEN_HEX, WATCH_APP_TARGET_SHELL, true },
    { "timer", "Timer", "Quick countdown presets", UI_COLOR_ACCENT_BLUE_HEX, WATCH_APP_TARGET_SHELL, true },
    { "sleep", "Sleep", "Schedule; sleep sensing unavailable", UI_COLOR_ACCENT_GREEN_HEX, WATCH_APP_TARGET_SHELL, true },
    { "wallet", "Wallet", "Cards and passes", UI_COLOR_ACCENT_BLUE_HEX, WATCH_APP_TARGET_SHELL, true },
    { "settings", "Settings", "Brightness, sound, and system", UI_COLOR_ACCENT_BLUE_HEX, WATCH_APP_TARGET_SETTINGS, true },
    { "weather", "Weather", "Current conditions and forecast", 0x30B0C7UL, WATCH_APP_TARGET_SHELL, true },
    { "messages", "Messages", "Recent conversations", 0x34C759UL, WATCH_APP_TARGET_SHELL, true },
    { "calendar", "Calendar", "Today and upcoming events", 0xFF453AUL, WATCH_APP_TARGET_SHELL, true },
    { "alarm", "Alarm", "Alarms and wake schedule", 0xFF9F0AUL, WATCH_APP_TARGET_SHELL, true },
    { "camera", "Camera", "Remote camera controls", 0x8E8E93UL, WATCH_APP_TARGET_SHELL, true },
    { "compass", "Compass", "Heading and elevation", 0x5E5CE6UL, WATCH_APP_TARGET_SHELL, true },
    { "activity", "Activity", "Motion sensor unavailable", 0xBF5AF2UL, WATCH_APP_TARGET_SHELL, true },
    { "calculator", "Calculator", "Quick calculations", 0xFF9F0AUL, WATCH_APP_TARGET_SHELL, true },
    { "find", "Find", "Locate people and devices", 0x64D2FFUL, WATCH_APP_TARGET_SHELL, true },
    { "silvercare", "SilverCare", "Medicine box companion", 0x24A878UL, WATCH_APP_TARGET_SILVERCARE, true },
};

typedef struct {
    bool initialized;
    const watch_app_descriptor_t *descriptors[UI_MAX_REGISTERED_APPS];
    size_t count;
} app_registry_state_t;

static app_registry_state_t s_registry_state;

static bool app_registry_ids_match(const char *left, const char *right);

/** {@inheritDoc app_registry_init} */
esp_err_t app_registry_init(void)
{
    memset(&s_registry_state, 0, sizeof(s_registry_state));
    s_registry_state.initialized = true;
    return ESP_OK;
}

/** {@inheritDoc app_registry_register} */
esp_err_t app_registry_register(const watch_app_descriptor_t *descriptor)
{
    size_t index;

    if(!s_registry_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if(descriptor == NULL || descriptor->app_id == NULL || descriptor->title == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for(index = 0; index < s_registry_state.count; ++index) {
        if(app_registry_ids_match(s_registry_state.descriptors[index]->app_id, descriptor->app_id)) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    if(s_registry_state.count >= UI_MAX_REGISTERED_APPS) {
        return ESP_ERR_NO_MEM;
    }

    s_registry_state.descriptors[s_registry_state.count] = descriptor;
    ++s_registry_state.count;
    return ESP_OK;
}

/** {@inheritDoc app_registry_register_builtin_apps} */
esp_err_t app_registry_register_builtin_apps(void)
{
    size_t index;

    if(!s_registry_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    for(index = 0; index < (sizeof(BUILTIN_APPS) / sizeof(BUILTIN_APPS[0])); ++index) {
        ESP_RETURN_ON_ERROR(app_registry_register(&BUILTIN_APPS[index]), "app_registry", "builtin app registration failed");
    }

    return ESP_OK;
}

/** {@inheritDoc app_registry_get_visible_count} */
size_t app_registry_get_visible_count(void)
{
    size_t index;
    size_t visible_count = 0U;

    for(index = 0; index < s_registry_state.count; ++index) {
        if(s_registry_state.descriptors[index]->visible_in_grid) {
            ++visible_count;
        }
    }

    return visible_count;
}

/** {@inheritDoc app_registry_get_visible_at} */
const watch_app_descriptor_t *app_registry_get_visible_at(size_t visible_index)
{
    size_t index;
    size_t current_visible_index = 0U;

    for(index = 0; index < s_registry_state.count; ++index) {
        if(!s_registry_state.descriptors[index]->visible_in_grid) {
            continue;
        }

        if(current_visible_index == visible_index) {
            return s_registry_state.descriptors[index];
        }

        ++current_visible_index;
    }

    return NULL;
}

/** {@inheritDoc app_registry_get_by_id} */
const watch_app_descriptor_t *app_registry_get_by_id(const char *app_id)
{
    size_t index;

    if(app_id == NULL) {
        return NULL;
    }

    for(index = 0; index < s_registry_state.count; ++index) {
        if(app_registry_ids_match(s_registry_state.descriptors[index]->app_id, app_id)) {
            return s_registry_state.descriptors[index];
        }
    }

    return NULL;
}

/** {@inheritDoc app_registry_get_total_count} */
size_t app_registry_get_total_count(void)
{
    return s_registry_state.count;
}

static bool app_registry_ids_match(const char *left, const char *right)
{
    return (left != NULL && right != NULL && strcmp(left, right) == 0);
}