#pragma once

#include <ArduinoJson.h>

namespace DisplayStudio::Scene {
class SceneModel {
public:
    static bool validate(
        JsonVariantConst scene,
        String& error
    );

    static String id(
        JsonVariantConst scene
    );

    static String name(
        JsonVariantConst scene
    );

    static bool enabled(
        JsonVariantConst scene
    );

    static JsonVariantConst config(
        JsonVariantConst scene
    );
};
}
