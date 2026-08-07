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

ProjectStorage& projectStorage() {
    return instance;
}

void ProjectStorage::begin() {
}

bool ProjectStorage::saveBlob(
    const uint8_t* data,
    size_t length
) {
    if (
        data == nullptr ||
        length == 0 ||
        length > Config::MAX_PROJECT_BYTES
    ) {
        Core::Logger::error(
            "[STORAGE] Invalid blob size: " + String(length)
        );
        return false;
    }

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) {
        Core::Logger::error("[STORAGE] NVS open failed");
        return false;
    }

    const size_t written = preferences.putBytes(
        KEY_PROJECT_BLOB,
        data,
        length
    );

    if (written == length) {
        preferences.remove(KEY_PROJECT_STRING);
        preferences.remove(KEY_LEGACY);
    }

    preferences.end();

    const bool ok = written == length;
    Core::Logger::info(
        ok
            ? "[STORAGE] Blob saved: " + String(length) +
                " bytes, freeHeap=" + String(ESP.getFreeHeap())
            : "[STORAGE] Blob save failed: wrote " +
                String(written) + "/" + String(length) + " bytes"
    );
    return ok;
}

bool ProjectStorage::loadBlob(
    uint8_t*& data,
    size_t& length
) {
    data = nullptr;
    length = 0;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) {
        Core::Logger::error("[STORAGE] NVS open failed");
        return false;
    }

    const size_t blobLength =
        preferences.getBytesLength(KEY_PROJECT_BLOB);

    if (blobLength == 0) {
        preferences.end();
        return false;
    }

    if (blobLength > Config::MAX_PROJECT_BYTES) {
        preferences.end();
        Core::Logger::error(
            "[STORAGE] Stored blob too large: " + String(blobLength)
        );
        return false;
    }

    uint8_t* payload = static_cast<uint8_t*>(
        malloc(blobLength + 1)
    );

    if (!payload) {
        preferences.end();
        Core::Logger::error(
            "[STORAGE] Blob allocation failed: " +
            String(blobLength + 1) +
            " bytes, freeHeap=" + String(ESP.getFreeHeap())
        );
        return false;
    }

    const size_t read = preferences.getBytes(
        KEY_PROJECT_BLOB,
        payload,
        blobLength
    );
    preferences.end();

    if (read != blobLength) {
        free(payload);
        Core::Logger::error(
            "[STORAGE] Blob read failed: " +
            String(read) + "/" + String(blobLength)
        );
        return false;
    }

    payload[blobLength] = 0;
    data = payload;
    length = blobLength;

    Core::Logger::info(
        "[STORAGE] Blob loaded: " + String(length) +
        " bytes, freeHeap=" + String(ESP.getFreeHeap())
    );
    return true;
}

bool ProjectStorage::verifyBlob(
    const uint8_t* expected,
    size_t length
) {
    if (expected == nullptr || length == 0) {
        Core::Logger::error("[STORAGE] Verify input invalid");
        return false;
    }

    uint8_t* actual = nullptr;
    size_t actualLength = 0;

    if (!loadBlob(actual, actualLength)) {
        Core::Logger::error("[STORAGE] Verify failed: reload failed");
        return false;
    }

    const bool ok =
        actualLength == length &&
        memcmp(actual, expected, length) == 0;

    free(actual);

    Core::Logger::info(
        ok
            ? "[STORAGE] Raw verify passed: " + String(length) + " bytes"
            : "[STORAGE] Raw verify failed: expected=" +
                String(length) + ", actual=" + String(actualLength)
    );
    return ok;
}

bool ProjectStorage::save(
    JsonVariantConst project
) {
    const size_t payloadLength = measureJson(project);

    if (
        payloadLength == 0 ||
        payloadLength > Config::MAX_PROJECT_BYTES
    ) {
        Core::Logger::error(
            "[STORAGE] Project size invalid: " + String(payloadLength)
        );
        return false;
    }

    char* payload = static_cast<char*>(
        malloc(payloadLength + 1)
    );

    if (!payload) {
        Core::Logger::error(
            "[STORAGE] Serialize buffer allocation failed: " +
            String(payloadLength + 1) +
            " bytes, freeHeap=" + String(ESP.getFreeHeap())
        );
        return false;
    }

    const size_t serialized = serializeJson(
        project,
        payload,
        payloadLength + 1
    );

    if (serialized != payloadLength) {
        free(payload);
        Core::Logger::error(
            "[STORAGE] Serialization length mismatch"
        );
        return false;
    }

    const bool ok = saveBlob(
        reinterpret_cast<const uint8_t*>(payload),
        payloadLength
    );
    free(payload);
    return ok;
}

bool ProjectStorage::load(
    JsonDocument& project
) {
    uint8_t* payload = nullptr;
    size_t payloadLength = 0;

    if (loadBlob(payload, payloadLength)) {
        const DeserializationError error = deserializeJson(
            project,
            reinterpret_cast<const char*>(payload),
            payloadLength
        );
        free(payload);

        if (error || project.overflowed()) {
            Core::Logger::error(
                "[STORAGE] Stored blob JSON invalid: " +
                String(error.c_str())
            );
            return false;
        }

        return true;
    }

    // Compatibility path for older string-based releases.
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) {
        Core::Logger::error("[STORAGE] NVS open failed");
        return false;
    }

    String serialized = preferences.getString(
        KEY_PROJECT_STRING,
        ""
    );

    if (serialized.isEmpty()) {
        serialized = preferences.getString(KEY_LEGACY, "");
    }

    preferences.end();

    if (serialized.isEmpty()) {
        Core::Logger::info("[STORAGE] No stored Project found");
        return false;
    }

    const DeserializationError error =
        deserializeJson(project, serialized);

    if (error || project.overflowed()) {
        Core::Logger::error(
            "[STORAGE] Legacy string JSON invalid: " +
            String(error.c_str())
        );
        return false;
    }

    Core::Logger::info(
        "[STORAGE] Legacy string loaded; migrates on next save"
    );
    return true;
}

bool ProjectStorage::verify(
    JsonVariantConst expected
) {
    const size_t payloadLength = measureJson(expected);

    if (
        payloadLength == 0 ||
        payloadLength > Config::MAX_PROJECT_BYTES
    ) {
        Core::Logger::error(
            "[STORAGE] Verify Project size invalid: " +
            String(payloadLength)
        );
        return false;
    }

    char* payload = static_cast<char*>(
        malloc(payloadLength + 1)
    );

    if (!payload) {
        Core::Logger::error(
            "[STORAGE] Verify buffer allocation failed: " +
            String(payloadLength + 1) +
            " bytes, freeHeap=" + String(ESP.getFreeHeap())
        );
        return false;
    }

    const size_t serialized = serializeJson(
        expected,
        payload,
        payloadLength + 1
    );

    if (serialized != payloadLength) {
        free(payload);
        Core::Logger::error(
            "[STORAGE] Verify serialization mismatch"
        );
        return false;
    }

    // Important: verification is byte-for-byte against NVS. We no longer
    // deserialize the stored Project into a second JsonDocument, which removes
    // one complete JSON tree from peak install memory.
    const bool ok = verifyBlob(
        reinterpret_cast<const uint8_t*>(payload),
        payloadLength
    );
    free(payload);
    return ok;
}

void ProjectStorage::clear() {
    Preferences preferences;
    if (preferences.begin(NAMESPACE, false)) {
        preferences.remove(KEY_PROJECT_BLOB);
        preferences.remove(KEY_PROJECT_STRING);
        preferences.remove(KEY_LEGACY);
        preferences.end();
    }
    Core::Logger::info("[STORAGE] Stored Project cleared");
}

bool ProjectStorage::exists() {
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) {
        return false;
    }

    const bool present =
        preferences.isKey(KEY_PROJECT_BLOB) ||
        preferences.isKey(KEY_PROJECT_STRING) ||
        preferences.isKey(KEY_LEGACY);

    preferences.end();
    return present;
}

}
