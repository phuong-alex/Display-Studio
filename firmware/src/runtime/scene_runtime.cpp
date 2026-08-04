#include <Arduino.h>
#include <time.h>

#include "display_studio/core/logger.h"
#include "display_studio/renderer/display_renderer.h"
#include "display_studio/runtime/runtime_queue.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/runtime/time_service.h"

namespace DisplayStudio::Runtime {
namespace {
SceneRuntime instance;
}

SceneRuntime& sceneRuntime() {
    return instance;
}

void SceneRuntime::begin() {
}

String SceneRuntime::preset() const {
    return String(
        static_cast<const char*>(
            activeConfig_["scene"]["preset"] |
            "clock-calendar"
        )
    );
}

uint32_t SceneRuntime::refreshMinutes() const {
    return constrain(
        static_cast<uint32_t>(
            activeConfig_["scene"]["runtime"]
                ["refreshMinutes"] | 5
        ),
        1UL,
        1440UL
    );
}

bool SceneRuntime::redAccent() const {
    return String(
        static_cast<const char*>(
            activeConfig_["scene"]["appearance"]
                ["accent"] | "red"
        )
    ) == "red";
}

String SceneRuntime::lunarText() const {
    return String(
        static_cast<const char*>(
            activeConfig_["scene"]["content"]
                ["lunarText"] | ""
        )
    );
}

bool SceneRuntime::configure(
    JsonVariantConst config
) {
    if (
        String(
            static_cast<const char*>(
                config["schema"] | ""
            )
        ) != "display-studio/device-config-v1"
    ) {
        Core::Logger::error(
            "Unsupported Project schema"
        );
        return false;
    }

    activeConfig_.clear();
    activeConfig_.set(config);
    configured_ = true;
    lastRenderBucket_ = UINT32_MAX;

    Core::Logger::info(
        "Scene runtime configured: " +
        preset() +
        ", refresh=" +
        String(refreshMinutes()) +
        " min"
    );
    return true;
}

bool SceneRuntime::configured() const {
    return configured_;
}

bool SceneRuntime::renderNow() {
    return runtimeQueue().enqueueRender();
}

bool SceneRuntime::renderImmediate() {
    if (!configured_) {
        Core::Logger::warning(
            "Cannot render: Project not configured"
        );
        return false;
    }

    tm value;

    if (!timeService().getLocalTime(value)) {
        Core::Logger::warning(
            "Cannot render: time not synchronized"
        );
        return false;
    }

    char hour[4];
    char minute[4];
    char weekday[20];
    char dateText[20];

    strftime(hour, sizeof(hour), "%H", &value);
    strftime(minute, sizeof(minute), "%M", &value);
    strftime(
        weekday,
        sizeof(weekday),
        "%A",
        &value
    );
    strftime(
        dateText,
        sizeof(dateText),
        "%d/%m/%Y",
        &value
    );

    const bool ok =
        Renderer::displayRenderer().showScene(
            preset(),
            hour,
            minute,
            weekday,
            dateText,
            lunarText(),
            redAccent()
        );

    Core::Logger::info(
        ok
            ? "Scene rendered"
            : "Scene render failed"
    );
    return ok;
}

void SceneRuntime::loop() {
    if (
        !configured_ ||
        !timeService().ready()
    ) {
        return;
    }

    const uint32_t localMinute =
        static_cast<uint32_t>(
            (
                time(nullptr) +
                timeService()
                    .timezoneOffsetMinutes() * 60
            ) / 60
        );

    const uint32_t bucket =
        localMinute / refreshMinutes();

    if (bucket == lastRenderBucket_) {
        return;
    }

    // Scheduler path also queues work; it never calls the driver.
    if (runtimeQueue().enqueueRender()) {
        lastRenderBucket_ = bucket;
    }
}

void SceneRuntime::clear() {
    activeConfig_.clear();
    configured_ = false;
    lastRenderBucket_ = UINT32_MAX;
}
}
