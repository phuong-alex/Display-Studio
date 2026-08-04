#include <Arduino.h>
#include <time.h>

#include "display.h"
#include "logger.h"
#include "studio_runtime.h"
#include "studio_time.h"

namespace {
JsonDocument activeConfig;
bool hasConfig = false;
uint32_t lastRenderBucket = UINT32_MAX;

String preset() {
    return String(
        static_cast<const char*>(
            activeConfig["scene"]["preset"] |
            "clock-calendar"
        )
    );
}

uint32_t refreshMinutes() {
    return constrain(
        static_cast<uint32_t>(
            activeConfig["scene"]
                ["runtime"]
                ["refreshMinutes"] | 5
        ),
        1UL,
        1440UL
    );
}

bool redAccent() {
    return String(
        static_cast<const char*>(
            activeConfig["scene"]
                ["appearance"]
                ["accent"] | "red"
        )
    ) == "red";
}

String lunarText() {
    return String(
        static_cast<const char*>(
            activeConfig["scene"]
                ["content"]
                ["lunarText"] | ""
        )
    );
}
}

namespace StudioRuntime {
void begin() {
}

bool configure(JsonVariantConst config) {
    if (
        String(
            static_cast<const char*>(
                config["schema"] | ""
            )
        ) != "display-studio/device-config-v1"
    ) {
        Logger::error(
            "Unsupported Studio config schema"
        );
        return false;
    }

    activeConfig.clear();
    activeConfig.set(config);
    hasConfig = true;
    lastRenderBucket = UINT32_MAX;

    Logger::info(
        "Studio runtime configured: " +
        preset() +
        ", refresh=" +
        String(refreshMinutes()) +
        " min"
    );

    return true;
}

bool configured() {
    return hasConfig;
}

bool renderNow() {
    if (!hasConfig) {
        Logger::warning(
            "Cannot render: Scene is not configured"
        );
        return false;
    }

    tm value;

    if (!StudioTime::getLocalTime(value)) {
        Logger::warning(
            "Cannot render: browser time not synchronized"
        );

        Display::showStudioWaitingTime();
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

    const bool ok = Display::showStudioScene(
        preset(),
        hour,
        minute,
        weekday,
        dateText,
        lunarText(),
        redAccent()
    );

    Logger::info(
        ok
            ? "Studio Scene rendered"
            : "Studio Scene render failed"
    );

    return ok;
}

void loop() {
    if (!hasConfig || !StudioTime::ready()) {
        return;
    }

    const uint32_t localMinute =
        static_cast<uint32_t>(
            (
                time(nullptr) +
                StudioTime::timezoneOffsetMinutes() * 60
            ) / 60
        );

    const uint32_t bucket =
        localMinute / refreshMinutes();

    if (bucket == lastRenderBucket) {
        return;
    }

    if (renderNow()) {
        lastRenderBucket = bucket;
    }
}

void clear() {
    activeConfig.clear();
    hasConfig = false;
    lastRenderBucket = UINT32_MAX;
}
}
