#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "display_studio/core/logger.h"
#include "display_studio/display/display_engine.h"
#include "display_studio/renderer/display_renderer.h"
#include "display_studio/runtime/scene_runtime.h"

namespace DisplayStudio::Display {
namespace {
constexpr uint32_t FULL_REFRESH_RECOVERY_MS = 5000;
constexpr uint32_t RECOVERY_SETTLE_MS = 1500;
constexpr TickType_t LOCK_TIMEOUT = pdMS_TO_TICKS(1000);

SemaphoreHandle_t refreshMutex = nullptr;
volatile DisplayEngineState engineState =
    DisplayEngineState::Stopped;
uint32_t refreshFinishedAt = 0;
uint32_t recoveryCount = 0;

DisplayEngine instance;
}

DisplayEngine& displayEngine() {
    return instance;
}

bool DisplayEngine::begin() {
    if (!refreshMutex) {
        refreshMutex = xSemaphoreCreateMutex();
    }

    if (!refreshMutex) {
        engineState = DisplayEngineState::Error;
        Core::Logger::error(
            "Display Engine mutex initialization failed"
        );
        return false;
    }

    engineState = DisplayEngineState::Idle;
    Core::Logger::info(
        "Display Engine 3.0 ready; recovery=" +
        String(FULL_REFRESH_RECOVERY_MS) +
        " ms, retry=1"
    );
    return true;
}

bool DisplayEngine::waitForRecoveryWindow() {
    if (refreshFinishedAt == 0) {
        return true;
    }

    const uint32_t elapsed =
        millis() - refreshFinishedAt;

    if (elapsed >= FULL_REFRESH_RECOVERY_MS) {
        return true;
    }

    const uint32_t remaining =
        FULL_REFRESH_RECOVERY_MS - elapsed;

    engineState = DisplayEngineState::CoolingDown;
    Core::Logger::info(
        "Display Engine cooling down: " +
        String(remaining) +
        " ms"
    );

    vTaskDelay(pdMS_TO_TICKS(remaining));
    return true;
}

bool DisplayEngine::renderActiveScene() {
    if (!refreshMutex) {
        Core::Logger::error(
            "Display Engine is not initialized"
        );
        return false;
    }

    if (
        xSemaphoreTake(
            refreshMutex,
            LOCK_TIMEOUT
        ) != pdTRUE
    ) {
        Core::Logger::warning(
            "Display Engine busy; refresh rejected"
        );
        return false;
    }

    waitForRecoveryWindow();

    engineState = DisplayEngineState::Refreshing;
    Core::Logger::info(
        "Display Engine refresh attempt 1/2"
    );

    bool ok =
        Runtime::sceneRuntime().renderImmediate();

    if (!ok) {
        engineState = DisplayEngineState::Error;
        recoveryCount++;

        Core::Logger::warning(
            "Display refresh failed; starting controller recovery #" +
            String(recoveryCount)
        );

        const bool recovered =
            Renderer::displayRenderer()
                .recoverController();

        if (recovered) {
            vTaskDelay(
                pdMS_TO_TICKS(
                    RECOVERY_SETTLE_MS
                )
            );

            engineState =
                DisplayEngineState::Refreshing;
            Core::Logger::info(
                "Display Engine refresh attempt 2/2"
            );

            ok = Runtime::sceneRuntime()
                .renderImmediate();
        } else {
            Core::Logger::error(
                "Display controller recovery unavailable"
            );
        }
    }

    refreshFinishedAt = millis();
    engineState = ok
        ? DisplayEngineState::Idle
        : DisplayEngineState::Error;

    Core::Logger::info(
        ok
            ? "Display Engine refresh completed"
            : "Display Engine refresh failed after recovery"
    );

    xSemaphoreGive(refreshMutex);

    if (!ok) {
        engineState = DisplayEngineState::Idle;
    }

    return ok;
}

DisplayEngineState DisplayEngine::state() const {
    return engineState;
}

uint32_t DisplayEngine::lastRefreshFinishedAt() const {
    return refreshFinishedAt;
}

}
