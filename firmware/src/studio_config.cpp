#include <Arduino.h>
#include <Preferences.h>

#include "logger.h"
#include "studio_config.h"

namespace {
constexpr const char* NAMESPACE = "displaystudio";
constexpr const char* KEY_CONFIG = "scene_config";
constexpr size_t MAX_CONFIG_BYTES = 8192;
}

namespace StudioConfig {
void begin() {
}

bool save(JsonVariantConst config) {
    String serialized;
    serializeJson(config, serialized);

    if (
        serialized.isEmpty() ||
        serialized.length() > MAX_CONFIG_BYTES
    ) {
        Logger::error(
            "Studio config size invalid: " +
            String(serialized.length())
        );
        return false;
    }

    Preferences preferences;
    preferences.begin(NAMESPACE, false);

    const size_t written = preferences.putString(
        KEY_CONFIG,
        serialized
    );

    preferences.end();

    const bool ok = written == serialized.length();

    Logger::info(
        ok
            ? "Studio config saved"
            : "Studio config save failed"
    );

    return ok;
}

bool load(JsonDocument& config) {
    Preferences preferences;
    preferences.begin(NAMESPACE, true);

    const String serialized =
        preferences.getString(KEY_CONFIG, "");

    preferences.end();

    if (serialized.isEmpty()) {
        return false;
    }

    const DeserializationError error =
        deserializeJson(config, serialized);

    if (error) {
        Logger::error(
            "Stored Studio config invalid"
        );
        return false;
    }

    Logger::info("Stored Studio config loaded");
    return true;
}

void clear() {
    Preferences preferences;
    preferences.begin(NAMESPACE, false);
    preferences.remove(KEY_CONFIG);
    preferences.end();

    Logger::info("Stored Studio config cleared");
}

bool exists() {
    Preferences preferences;
    preferences.begin(NAMESPACE, true);
    const bool present =
        preferences.isKey(KEY_CONFIG);
    preferences.end();
    return present;
}
}
