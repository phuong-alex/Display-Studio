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
constexpr const char* KEY_PROJECT =
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
    String serialized;
    serializeJson(project, serialized);

    if (
        serialized.isEmpty() ||
        serialized.length() >
            Config::MAX_PROJECT_BYTES
    ) {
        Core::Logger::error(
            "Project size invalid: " +
            String(serialized.length())
        );
        return false;
    }

    Preferences preferences;
    preferences.begin(NAMESPACE, false);

    const size_t written =
        preferences.putString(
            KEY_PROJECT,
            serialized
        );

    preferences.end();

    const bool ok =
        written == serialized.length();

    Core::Logger::info(
        ok
            ? "Project saved"
            : "Project save failed"
    );

    return ok;
}

bool ProjectStorage::load(
    JsonDocument& project
) {
    Preferences preferences;
    preferences.begin(NAMESPACE, true);

    String serialized =
        preferences.getString(
            KEY_PROJECT,
            ""
        );

    if (serialized.isEmpty()) {
        serialized =
            preferences.getString(
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
        deserializeJson(
            project,
            serialized
        );

    if (error) {
        Core::Logger::error(
            "Stored Project is invalid"
        );
        return false;
    }

    Core::Logger::info(
        "Stored Project loaded"
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
        Project::ProjectChecksum::calculate(
            expected
        );

    const uint32_t actualChecksum =
        Project::ProjectChecksum::calculate(
            actual.as<JsonVariantConst>()
        );

    const bool ok =
        expectedChecksum == actualChecksum;

    Core::Logger::info(
        "Project verify: expected=" +
        Project::ProjectChecksum::hex(
            expectedChecksum
        ) +
        ", actual=" +
        Project::ProjectChecksum::hex(
            actualChecksum
        )
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
    preferences.remove(KEY_PROJECT);
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
        preferences.isKey(KEY_PROJECT) ||
        preferences.isKey(KEY_LEGACY);

    preferences.end();

    return present;
}
}
