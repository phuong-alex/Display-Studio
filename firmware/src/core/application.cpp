#include <Arduino.h>
#include <ArduinoJson.h>

#include "display_studio/config.h"
#include "display_studio/core/application.h"
#include "display_studio/core/logger.h"
#include "display_studio/device/device_identity.h"
#include "display_studio/project/project_manager.h"
#include "display_studio/renderer/display_renderer.h"
#include "display_studio/runtime/runtime_queue.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/runtime/time_service.h"
#include "display_studio/storage/project_storage.h"
#include "display_studio/transport/ble_transport.h"
#include "display_studio/version.h"

namespace DisplayStudio::Core {
namespace {
Application instance;
bool displayReady = false;
}

Application& application() {
    return instance;
}

void Application::setup() {
    Logger::begin(Config::SERIAL_BAUD);
    Logger::info(Version::PRODUCT);
    Logger::info(Version::FIRMWARE);
    Logger::info("Display Studio 2.1 Runtime Queue");

    Device::deviceIdentity().begin();
    Storage::projectStorage().begin();
    Runtime::timeService().begin();
    Runtime::sceneRuntime().begin();
    Project::projectManager().begin();

    // Transport is available before display and storage work.
    Transport::bleTransport().begin();
    Logger::info("V2 transport online");

    displayReady = Renderer::displayRenderer().begin();
    if (!displayReady) {
        Logger::error(
            "Display initialization failed; BLE remains available"
        );
    }

    if (!Runtime::runtimeQueue().begin()) {
        Logger::error(
            "Runtime Queue failed; render requests unavailable"
        );
    }

    // Loading configures only the active Scene. It does not render.
    if (Project::projectManager().load()) {
        Logger::info(
            "Stored Project ready; waiting for Runtime operation"
        );
    } else {
        Logger::info(
            "No Project installed; waiting for Studio"
        );
    }

    Logger::info(
        "Display Studio 2.1 queued runtime ready"
    );
}

void Application::loop() {
    // Blocking display work runs in the dedicated Renderer Worker.
    // The Arduino loop stays available for BLE ACK and response pumping.
    Transport::bleTransport().loop();
    delay(2);
}
}
