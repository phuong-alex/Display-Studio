#pragma once

#include <ArduinoJson.h>

namespace DisplayStudio::Runtime {
class SceneRuntime {
public:
    void begin();
    bool configure(JsonVariantConst config);
    bool configured() const;

    // Public callers request work through Runtime Queue.
    bool renderNow();

    // Renderer Worker is the only caller allowed to execute
    // the physical display operation.
    bool renderImmediate();

    void loop();
    void clear();

private:
    JsonDocument activeConfig_;
    bool configured_ = false;
    uint32_t lastRenderBucket_ = UINT32_MAX;

    String preset() const;
    uint32_t refreshMinutes() const;
    bool redAccent() const;
    String lunarText() const;
};

SceneRuntime& sceneRuntime();
}
