#include <Arduino.h>
#include <ArduinoJson.h>

#include "display_studio/scene/scene_model.h"

namespace DisplayStudio::Scene {
namespace {
constexpr size_t MAX_SCENE_ID_LENGTH = 48;
constexpr size_t MAX_SCENE_NAME_LENGTH = 80;
}

bool SceneModel::validate(
    JsonVariantConst scene,
    String& error
) {
    if (!scene.is<JsonObjectConst>()) {
        error = "scene_must_be_object";
        return false;
    }

    const String sceneId = id(scene);

    if (sceneId.isEmpty()) {
        error = "scene_id_required";
        return false;
    }

    if (sceneId.length() > MAX_SCENE_ID_LENGTH) {
        error = "scene_id_too_long";
        return false;
    }

    const String sceneName = name(scene);

    if (sceneName.isEmpty()) {
        error = "scene_name_required";
        return false;
    }

    if (sceneName.length() > MAX_SCENE_NAME_LENGTH) {
        error = "scene_name_too_long";
        return false;
    }

    if (!scene["config"].is<JsonObjectConst>()) {
        error = "scene_config_required";
        return false;
    }

    error = "";
    return true;
}

String SceneModel::id(
    JsonVariantConst scene
) {
    return String(
        static_cast<const char*>(
            scene["id"] | ""
        )
    );
}

String SceneModel::name(
    JsonVariantConst scene
) {
    return String(
        static_cast<const char*>(
            scene["name"] | ""
        )
    );
}

bool SceneModel::enabled(
    JsonVariantConst scene
) {
    return scene["enabled"] | true;
}

JsonVariantConst SceneModel::config(
    JsonVariantConst scene
) {
    return scene["config"];
}
}
