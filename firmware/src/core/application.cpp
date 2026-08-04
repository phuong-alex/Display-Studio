#include <Arduino.h>
#include <ArduinoJson.h>

#include "display_studio/config.h"
#include "display_studio/core/application.h"
#include "display_studio/core/logger.h"
#include "display_studio/device/device_identity.h"
#include "display_studio/project/project_manager.h"
#include "display_studio/renderer/display_renderer.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/runtime/time_service.h"
#include "display_studio/storage/project_storage.h"
#include "display_studio/transport/ble_transport.h"
#include "display_studio/version.h"

namespace DisplayStudio::Core {
namespace {
Application instance;
}

Application& application() {
    return instance;
}

void Application::setup() {
    Logger::begin(Config::SERIAL_BAUD);
    Logger::info(Version::PRODUCT);
    Logger::info(Version::FIRMWARE);
    Logger::info("Scene Engine Foundation Sprint 1.4");

    Device::deviceIdentity().begin();
    Storage::projectStorage().begin();
    Runtime::timeService().begin();
    Runtime::sceneRuntime().begin();
    Project::projectManager().begin();

    if (!Renderer::displayRenderer().begin()) {
        Logger::error(
            "Display initialization failed"
        );
        return;
    }

    if (
        Project::projectManager().load()
    ) {
        Renderer::displayRenderer()
            .showWaitingForTime();
    } else {
        Renderer::displayRenderer()
            .showBootScreen();
    }

    Transport::bleTransport().begin();

    Logger::info(
        "Sprint 1.4 Scene runtime ready"
    );
}

void Application::loop() {
    Transport::bleTransport().loop();
    Runtime::sceneRuntime().loop();
    delay(20);
}
}
