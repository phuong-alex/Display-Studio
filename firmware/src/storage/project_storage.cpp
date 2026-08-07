#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <cstring>

#include "display_studio/config.h"
#include "display_studio/core/logger.h"
#include "display_studio/storage/project_storage.h"

namespace DisplayStudio::Storage {
namespace {
constexpr const char* PROJECT_PATH = "/project.json";
constexpr const char* PROJECT_TEMP_PATH = "/project.tmp";

// Legacy NVS keys are read only for migration compatibility.
constexpr const char* NAMESPACE = "displaystudio";
constexpr const char* KEY_PROJECT_BLOB = "project_blob";
constexpr const char* KEY_PROJECT_STRING = "project_v1";
constexpr const char* KEY_LEGACY = "scene_config";

ProjectStorage instance;
bool littleFsReady = false;

bool ensureLittleFs() {
    if (littleFsReady) {
        return true;
    }

    if (LittleFS.begin(false)) {
        littleFsReady = true;
    } else {
        Core::Logger::warning(
            "[STORAGE] LittleFS mount failed; formatting filesystem"
        );
        littleFsReady = LittleFS.begin(true);
    }

    if (!littleFsReady) {
        Core::Logger::error("[STORAGE] LittleFS initialization failed");
        return false;
    }

    Core::Logger::info(
        "[STORAGE] LittleFS ready: total=" +
        String(LittleFS.totalBytes()) +
        ", used=" + String(LittleFS.usedBytes()) +
        ", free=" + String(LittleFS.totalBytes() - LittleFS.usedBytes())
    );
    return true;
}

bool loadLegacyNvsBlob(uint8_t*& data, size_t& length) {
    data = nullptr;
    length = 0;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) {
        return false;
    }

    const size_t blobLength = preferences.getBytesLength(KEY_PROJECT_BLOB);
    if (blobLength == 0 || blobLength > Config::MAX_PROJECT_BYTES) {
        preferences.end();
        return false;
    }

    uint8_t* payload = static_cast<uint8_t*>(malloc(blobLength + 1));
    if (!payload) {
        preferences.end();
        Core::Logger::error(
            "[STORAGE] Legacy NVS blob allocation failed: " +
            String(blobLength + 1)
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
        return false;
    }

    payload[blobLength] = 0;
    data = payload;
    length = blobLength;
    Core::Logger::info(
        "[STORAGE] Legacy NVS blob loaded: " +
        String(blobLength) + " bytes; migrates to LittleFS on next save"
    );
    return true;
}

void clearLegacyNvsProject() {
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) {
        return;
    }
    preferences.remove(KEY_PROJECT_BLOB);
    preferences.remove(KEY_PROJECT_STRING);
    preferences.remove(KEY_LEGACY);
    preferences.end();
}
}

ProjectStorage& projectStorage() {
    return instance;
}

void ProjectStorage::begin() {
    ensureLittleFs();
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

    if (!ensureLittleFs()) {
        return false;
    }

    const size_t freeBytes =
        LittleFS.totalBytes() - LittleFS.usedBytes();

    if (freeBytes < length + 4096) {
        Core::Logger::error(
            "[STORAGE] LittleFS not enough space: need=" +
            String(length) + ", free=" + String(freeBytes)
        );
        return false;
    }

    LittleFS.remove(PROJECT_TEMP_PATH);

    File file = LittleFS.open(PROJECT_TEMP_PATH, FILE_WRITE);
    if (!file) {
        Core::Logger::error("[STORAGE] Project temp file open failed");
        return false;
    }

    const size_t written = file.write(data, length);
    file.flush();
    file.close();

    if (written != length) {
        LittleFS.remove(PROJECT_TEMP_PATH);
        Core::Logger::error(
            "[STORAGE] LittleFS write failed: wrote=" +
            String(written) + "/" + String(length)
        );
        return false;
    }

    // Replace only after a complete write, so a failed save cannot destroy
    // the last known-good Project.
    LittleFS.remove(PROJECT_PATH);
    if (!LittleFS.rename(PROJECT_TEMP_PATH, PROJECT_PATH)) {
        LittleFS.remove(PROJECT_TEMP_PATH);
        Core::Logger::error("[STORAGE] Project file commit failed");
        return false;
    }

    // Once LittleFS owns the Project, release obsolete NVS storage.
    clearLegacyNvsProject();

    Core::Logger::info(
        "[STORAGE] LittleFS Project saved: " +
        String(length) +
        " bytes, fsFree=" +
        String(LittleFS.totalBytes() - LittleFS.usedBytes()) +
        ", freeHeap=" + String(ESP.getFreeHeap())
    );
    return true;
}

bool ProjectStorage::loadBlob(
    uint8_t*& data,
    size_t& length
) {
    data = nullptr;
    length = 0;

    if (ensureLittleFs() && LittleFS.exists(PROJECT_PATH)) {
        File file = LittleFS.open(PROJECT_PATH, FILE_READ);
        if (!file) {
            Core::Logger::error("[STORAGE] Project file open failed");
            return false;
        }

        const size_t fileLength = file.size();
        if (fileLength == 0 || fileLength > Config::MAX_PROJECT_BYTES) {
            file.close();
            Core::Logger::error(
                "[STORAGE] Stored Project file size invalid: " +
                String(fileLength)
            );
            return false;
        }

        uint8_t* payload = static_cast<uint8_t*>(malloc(fileLength + 1));
        if (!payload) {
            file.close();
            Core::Logger::error(
                "[STORAGE] Project file allocation failed: " +
                String(fileLength + 1) +
                ", freeHeap=" + String(ESP.getFreeHeap())
            );
            return false;
        }

        const size_t read = file.read(payload, fileLength);
        file.close();

        if (read != fileLength) {
            free(payload);
            Core::Logger::error(
                "[STORAGE] Project file read failed: " +
                String(read) + "/" + String(fileLength)
            );
            return false;
        }

        payload[fileLength] = 0;
        data = payload;
        length = fileLength;

        Core::Logger::info(
            "[STORAGE] LittleFS Project loaded: " +
            String(length) +
            " bytes, freeHeap=" + String(ESP.getFreeHeap())
        );
        return true;
    }

    // Migration path for devices upgraded from NVS Project storage.
    return loadLegacyNvsBlob(data, length);
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

    char* payload = static_cast<char*>(malloc(payloadLength + 1));
    if (!payload) {
        Core::Logger::error(
            "[STORAGE] Serialize buffer allocation failed: " +
            String(payloadLength + 1) +
            ", freeHeap=" + String(ESP.getFreeHeap())
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
        Core::Logger::error("[STORAGE] Serialization length mismatch");
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
                "[STORAGE] Stored Project JSON invalid: " +
                String(error.c_str())
            );
            return false;
        }
        return true;
    }

    // Final compatibility path for very old string-based releases.
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) {
        Core::Logger::info("[STORAGE] No stored Project found");
        return false;
    }

    String serialized = preferences.getString(KEY_PROJECT_STRING, "");
    if (serialized.isEmpty()) {
        serialized = preferences.getString(KEY_LEGACY, "");
    }
    preferences.end();

    if (serialized.isEmpty()) {
        Core::Logger::info("[STORAGE] No stored Project found");
        return false;
    }

    const DeserializationError error = deserializeJson(project, serialized);
    if (error || project.overflowed()) {
        Core::Logger::error(
            "[STORAGE] Legacy string JSON invalid: " +
            String(error.c_str())
        );
        return false;
    }

    Core::Logger::info(
        "[STORAGE] Legacy string loaded; migrates to LittleFS on next save"
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

    char* payload = static_cast<char*>(malloc(payloadLength + 1));
    if (!payload) {
        Core::Logger::error(
            "[STORAGE] Verify buffer allocation failed: " +
            String(payloadLength + 1) +
            ", freeHeap=" + String(ESP.getFreeHeap())
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
        Core::Logger::error("[STORAGE] Verify serialization mismatch");
        return false;
    }

    const bool ok = verifyBlob(
        reinterpret_cast<const uint8_t*>(payload),
        payloadLength
    );
    free(payload);
    return ok;
}

void ProjectStorage::clear() {
    if (ensureLittleFs()) {
        LittleFS.remove(PROJECT_TEMP_PATH);
        LittleFS.remove(PROJECT_PATH);
    }
    clearLegacyNvsProject();
    Core::Logger::info("[STORAGE] Stored Project cleared");
}

bool ProjectStorage::exists() {
    if (ensureLittleFs() && LittleFS.exists(PROJECT_PATH)) {
        return true;
    }

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
