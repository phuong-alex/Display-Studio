#pragma once

#include <ArduinoJson.h>

namespace DisplayStudio::Project {
class ProjectModel {
public:
    static bool isProject(
        JsonVariantConst value
    );

    static bool isLegacyConfig(
        JsonVariantConst value
    );

    static bool migrateLegacy(
        JsonVariantConst legacy,
        JsonDocument& project
    );

    static bool validate(
        JsonVariantConst project,
        String& error
    );

    static JsonVariantConst activeScene(
        JsonVariantConst project
    );

    static bool setActiveScene(
        JsonDocument& project,
        const String& sceneId
    );
};
}
