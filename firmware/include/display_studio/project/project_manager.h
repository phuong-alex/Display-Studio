#pragma once

#include <ArduinoJson.h>

namespace DisplayStudio::Project {
class ProjectManager {
public:
    void begin();

    bool load();
    bool install(JsonVariantConst project);
    bool installLegacy(JsonVariantConst legacy);

    bool activateScene(
        const String& sceneId
    );

    bool installed() const;
    JsonVariantConst document() const;
    JsonVariantConst activeScene() const;

    void clear();

private:
    JsonDocument project_;
    bool installed_ = false;
};

ProjectManager& projectManager();
}
