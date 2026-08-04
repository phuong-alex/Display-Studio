#include <Arduino.h>
#include <Preferences.h>

#include "display_studio/config.h"
#include "display_studio/core/logger.h"
#include "display_studio/project/project_checksum.h"
#include "display_studio/storage/project_storage.h"

namespace DisplayStudio::Storage {
namespace {
constexpr const char* NAMESPACE =
    "displaystudio";
constexpr const char* KEY_PROJECT_BLOB =
    "project_blob";
constexpr const char* KEY_PROJECT_STRING =
    "project_v1";
constexpr const char* KEY_LEGACY =
    "scene_config";

ProjectStorage instance;
}

ProjectStorage& projectStorage() {
    return instance;
}

void ProjectStorage::begin() {
}

bool ProjectStorage::save(
    JsonVariantConst project
) {
    const size_t payloadLength =
        measureJson(project);

    if (
        payloadLength == 0 ||
        payloadLength > Config::MAX_PROJECT_BYTES
    ) {
        Core::Logger::error(
            "Project size invalid: " +
            String(payloadLength)
        );
        return false;
    }

    char* payload = static_cast<char*>(
        malloc(payloadLength + 1)
    );

    if (!payload) {
        Core::Logger::error(
            "Project buffer allocation failed: " +
            String(payloadLength + 1) +
            " bytes, freeHeap=" +
            String(ESP.getFreeHeap())
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
            "Project serialization length mismatch"
        );
        return false;
    }

    Preferences preferences;

    if (!preferences.begin(NAMESPACE, false)) {
        free(payload);
        Core::Logger::error(
            "Project NVS open failed"
        );
        return false;
    }

    const size_t written = preferences.putBytes(
        KEY_PROJECT_BLOB,
        payload,
        payloadLength
    );

    if (written == payloadLength) {
        // Remove older string-based representations after successful blob save.
        preferences.remove(KEY_PROJECT_STRING);
        preferences.remove(KEY_LEGACY);
    }

    preferences.end();
    free(payload);

    const bool ok = written == payloadLength;

    Core::Logger::info(
        ok
            ? "Project blob saved: " +
                String(payloadLength) +
                " bytes, freeHeap=" +
                String(ESP.getFreeHeap())
            : "Project blob save failed: wrote " +
                String(written) +
                "/" +
                String(payloadLength) +
                " bytes"
    );

    return ok;
}

bool ProjectStorage::load(
    JsonDocument& project
) {
    Preferences preferences;

    if (!preferences.begin(NAMESPACE, true)) {
        Core::Logger::error(
            "Project NVS open failed"
        );
        return false;
    }

    const size_t blobLength =
        preferences.getBytesLength(KEY_PROJECT_BLOB);

    if (blobLength > 0) {
        if (blobLength > Config::MAX_PROJECT_BYTES) {
            preferences.end();
            Core::Logger::error(
                "Stored Project blob too large: " +
                String(blobLength)
            );
            return false;
        }

        char* payload = static_cast<char*>(
            malloc(blobLength + 1)
        );

        if (!payload) {
            preferences.end();
            Core::Logger::error(
                "Stored Project buffer allocation failed: " +
                String(blobLength + 1) +
                " bytes, freeHeap=" +
                String(ESP.getFreeHeap())
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
                "Stored Project blob read failed: " +
                String(read) +
                "/" +
                String(blobLength)
            );
            return false;
        }

        payload[blobLength] = '\0';

        const DeserializationError error =
            deserializeJson(project, payload, blobLength);

        free(payload);

        if (error) {
            Core::Logger::error(
                "Stored Project blob is invalid: " +
                String(error.c_str())
            );
            return false;
        }

        Core::Logger::info(
            "Stored Project blob loaded: " +
            String(blobLength) +
            " bytes, freeHeap=" +
            String(ESP.getFreeHeap())
        );
        return true;
    }

    String serialized = preferences.getString(
        KEY_PROJECT_STRING,
        ""
    );

    if (serialized.isEmpty()) {
        serialized = preferences.getString(
            KEY_LEGACY,
            ""
        );
    }

    preferences.end();

    if (serialized.isEmpty()) {
        Core::Logger::info(
            "No stored Project found"
        );
        return false;
    }

    const DeserializationError error =
        deserializeJson(project, serialized);

    if (error) {
        Core::Logger::error(
            "Stored string Project is invalid: " +
            String(error.c_str())
        );
        return false;
    }

    Core::Logger::info(
        "Stored string Project loaded; migrating on next save"
    );

    return true;
}

bool ProjectStorage::verify(
    JsonVariantConst expected
) {
    JsonDocument actual;

    if (!load(actual)) {
        Core::Logger::error(
            "Project verify failed: reload failed"
        );
        return false;
    }

    const uint32_t expectedChecksum =
        Project::ProjectChecksum::calculate(expected);

    const uint32_t actualChecksum =
        Project::ProjectChecksum::calculate(
            actual.as<JsonVariantConst>()
        );

    const bool ok =
        expectedChecksum == actualChecksum;

    Core::Logger::info(
        "Project verify: expected=" +
        Project::ProjectChecksum::hex(expectedChecksum) +
        ", actual=" +
        Project::ProjectChecksum::hex(actualChecksum)
    );

    Core::Logger::info(
        ok
            ? "Project verify passed"
            : "Project verify failed"
    );

    return ok;
}

void ProjectStorage::clear() {
    Preferences preferences;
    preferences.begin(NAMESPACE, false);
    preferences.remove(KEY_PROJECT_BLOB);
    preferences.remove(KEY_PROJECT_STRING);
    preferences.remove(KEY_LEGACY);
    preferences.end();

    Core::Logger::info(
        "Stored Project cleared"
    );
}

bool ProjectStorage::exists() {
    Preferences preferences;
    preferences.begin(NAMESPACE, true);

    const bool present =
        preferences.isKey(KEY_PROJECT_BLOB) ||
        preferences.isKey(KEY_PROJECT_STRING) ||
        preferences.isKey(KEY_LEGACY);

    preferences.end();

    return present;
}
}
