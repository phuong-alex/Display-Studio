#include <Arduino.h>
#include <Preferences.h>
#include <cstring>

#include "display_studio/config.h"
#include "display_studio/core/logger.h"
#include "display_studio/storage/project_storage.h"

namespace DisplayStudio::Storage {
namespace {
constexpr const char* NAMESPACE = "displaystudio";
constexpr const char* KEY_PROJECT_BLOB = "project_blob";
constexpr const char* KEY_PROJECT_STRING = "project_v1";
constexpr const char* KEY_LEGACY = "scene_config";
ProjectStorage instance;
}

ProjectStorage& projectStorage() { return instance; }
void ProjectStorage::begin() {}

bool ProjectStorage::save(JsonVariantConst project) {
    const size_t length = measureJson(project);
    if (length == 0 || length > Config::MAX_PROJECT_BYTES) return false;

    char* payload = static_cast<char*>(malloc(length + 1));
    if (!payload) return false;
    const size_t serialized = serializeJson(project, payload, length + 1);
    if (serialized != length) {
        free(payload);
        return false;
    }

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) {
        free(payload);
        return false;
    }
    const size_t written = preferences.putBytes(KEY_PROJECT_BLOB, payload, length);
    if (written == length) {
        preferences.remove(KEY_PROJECT_STRING);
        preferences.remove(KEY_LEGACY);
    }
    preferences.end();
    free(payload);

    Core::Logger::info("[STORAGE] blob save " + String(written) + "/" + String(length));
    return written == length;
}

bool ProjectStorage::load(JsonDocument& project) {
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) return false;

    const size_t blobLength = preferences.getBytesLength(KEY_PROJECT_BLOB);
    if (blobLength > 0) {
        if (blobLength > Config::MAX_PROJECT_BYTES) {
            preferences.end();
            return false;
        }
        char* payload = static_cast<char*>(malloc(blobLength + 1));
        if (!payload) {
            preferences.end();
            return false;
        }
        const size_t read = preferences.getBytes(KEY_PROJECT_BLOB, payload, blobLength);
        preferences.end();
        if (read != blobLength) {
            free(payload);
            return false;
        }
        payload[blobLength] = '\0';
        const DeserializationError error = deserializeJson(project, payload, blobLength);
        free(payload);
        if (error || project.overflowed()) {
            Core::Logger::error("[STORAGE] blob JSON invalid: " + String(error.c_str()));
            return false;
        }
        Core::Logger::info("[STORAGE] blob loaded: " + String(blobLength) + " bytes");
        return true;
    }

    String serialized = preferences.getString(KEY_PROJECT_STRING, "");
    if (serialized.isEmpty()) serialized = preferences.getString(KEY_LEGACY, "");
    preferences.end();
    if (serialized.isEmpty()) return false;
    const DeserializationError error = deserializeJson(project, serialized);
    return !error && !project.overflowed();
}

bool ProjectStorage::verify(JsonVariantConst expected) {
    const size_t expectedLength = measureJson(expected);
    if (expectedLength == 0) return false;

    char* expectedBytes = static_cast<char*>(malloc(expectedLength + 1));
    if (!expectedBytes) return false;
    if (serializeJson(expected, expectedBytes, expectedLength + 1) != expectedLength) {
        free(expectedBytes);
        return false;
    }

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) {
        free(expectedBytes);
        return false;
    }
    const size_t storedLength = preferences.getBytesLength(KEY_PROJECT_BLOB);
    if (storedLength != expectedLength) {
        preferences.end();
        free(expectedBytes);
        Core::Logger::error("[STORAGE] verify length mismatch");
        return false;
    }

    uint8_t* storedBytes = static_cast<uint8_t*>(malloc(storedLength));
    if (!storedBytes) {
        preferences.end();
        free(expectedBytes);
        return false;
    }
    const size_t read = preferences.getBytes(KEY_PROJECT_BLOB, storedBytes, storedLength);
    preferences.end();

    const bool ok = read == storedLength &&
        memcmp(expectedBytes, storedBytes, storedLength) == 0;
    free(storedBytes);
    free(expectedBytes);

    Core::Logger::info(ok ? "[STORAGE] raw blob verify PASS" : "[STORAGE] raw blob verify FAIL");
    return ok;
}

void ProjectStorage::clear() {
    Preferences preferences;
    preferences.begin(NAMESPACE, false);
    preferences.remove(KEY_PROJECT_BLOB);
    preferences.remove(KEY_PROJECT_STRING);
    preferences.remove(KEY_LEGACY);
    preferences.end();
}

bool ProjectStorage::exists() {
    Preferences preferences;
    preferences.begin(NAMESPACE, true);
    const bool present = preferences.isKey(KEY_PROJECT_BLOB) ||
        preferences.isKey(KEY_PROJECT_STRING) || preferences.isKey(KEY_LEGACY);
    preferences.end();
    return present;
}

}
