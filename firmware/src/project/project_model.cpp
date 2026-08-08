#include <Arduino.h>
#include <ArduinoJson.h>

#include "display_studio/project/project_model.h"
#include "display_studio/scene/scene_model.h"

namespace DisplayStudio::Project {
namespace {
constexpr const char* PROJECT_SCHEMA =
    "display-studio/project-v1";

constexpr const char* LEGACY_SCHEMA =
    "display-studio/device-config-v1";
}

bool ProjectModel::isProject(
    JsonVariantConst value
) {
    return String(
        static_cast<const char*>(
            value["schema"] | ""
        )
    ) == PROJECT_SCHEMA;
}

bool ProjectModel::isLegacyConfig(
    JsonVariantConst value
) {
    return String(
        static_cast<const char*>(
            value["schema"] | ""
        )
    ) == LEGACY_SCHEMA;
}

bool ProjectModel::migrateLegacy(
    JsonVariantConst legacy,
    JsonDocument& project
) {
    if (!isLegacyConfig(legacy)) {
        return false;
    }

    project.clear();

    project["schema"] = PROJECT_SCHEMA;
    project["projectVersion"] = 1;
    project["name"] = "Migrated Project";
    project["activeSceneId"] = "main";

    JsonObject metadata =
        project["metadata"].to<JsonObject>();

    metadata["createdBy"] =
        "display-studio-migration";
    metadata["sourceSchema"] =
        LEGACY_SCHEMA;

    JsonArray scenes =
        project["scenes"].to<JsonArray>();

    JsonObject scene =
        scenes.add<JsonObject>();

    scene["id"] = "main";
    scene["name"] = "Main Scene";
    scene["enabled"] = true;
    scene["config"].set(legacy);

    return true;
}

bool ProjectModel::validate(
    JsonVariantConst project,
    String& error
) {
    if (!isProject(project)) {
        error = "invalid_project_schema";
        return false;
    }

    const String name = String(
        static_cast<const char*>(
            project["name"] | ""
        )
    );

    if (name.isEmpty()) {
        error = "project_name_required";
        return false;
    }

    const JsonArrayConst scenes =
        project["scenes"].as<JsonArrayConst>();

    if (scenes.isNull() || scenes.size() == 0) {
        error = "project_requires_scene";
        return false;
    }

    // Scene count is intentionally not capped here. ProjectModel validates
    // schema and semantic correctness only. Runtime/storage limits are enforced
    // by their owning layers (upload size, heap availability and LittleFS).
    // This keeps the Project schema portable across different Runtime targets.

    const String activeSceneId = String(
        static_cast<const char*>(
            project["activeSceneId"] | ""
        )
    );

    bool activeFound = false;

    for (size_t index = 0; index < scenes.size(); index++) {
        const JsonVariantConst scene = scenes[index];
        String sceneError;

        if (!Scene::SceneModel::validate(scene, sceneError)) {
            error =
                "invalid_scene_" +
                String(index) +
                ":" +
                sceneError;
            return false;
        }

        const String sceneId =
            Scene::SceneModel::id(scene);

        for (size_t previous = 0; previous < index; previous++) {
            if (
                Scene::SceneModel::id(scenes[previous]) ==
                sceneId
            ) {
                error = "duplicate_scene_id";
                return false;
            }
        }

        if (sceneId == activeSceneId) {
            if (!Scene::SceneModel::enabled(scene)) {
                error = "active_scene_disabled";
                return false;
            }

            activeFound = true;
        }
    }

    if (!activeFound) {
        error = "active_scene_not_found";
        return false;
    }

    error = "";
    return true;
}

JsonVariantConst ProjectModel::activeScene(
    JsonVariantConst project
) {
    const String activeSceneId = String(
        static_cast<const char*>(
            project["activeSceneId"] | ""
        )
    );

    for (
        JsonVariantConst scene :
        project["scenes"].as<JsonArrayConst>()
    ) {
        if (
            Scene::SceneModel::id(scene) == activeSceneId &&
            Scene::SceneModel::enabled(scene)
        ) {
            return scene;
        }
    }

    return JsonVariantConst();
}

bool ProjectModel::setActiveScene(
    JsonDocument& project,
    const String& sceneId
) {
    for (
        JsonVariantConst scene :
        project["scenes"].as<JsonArrayConst>()
    ) {
        if (
            Scene::SceneModel::id(scene) == sceneId &&
            Scene::SceneModel::enabled(scene)
        ) {
            project["activeSceneId"] = sceneId;
            return true;
        }
    }

    return false;
}
}
