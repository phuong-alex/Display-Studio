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
bool displayReady = false;
}

Application& application() {
    return instance;
}

void Application::setup() {
    Logger::begin(Config::SERIAL_BAUD);
    Logger::info(Version::PRODUCT);
    Logger::info(Version::FIRMWARE);
    Logger::info("Display Studio 2.0 Core Isolation");

    Device::deviceIdentity().begin();
    Storage::projectStorage().begin();
    Runtime::timeService().begin();
    Runtime::sceneRuntime().begin();
    Project::projectManager().begin();

    // V2 rule: transport is online before display or storage work can block.
    // The browser must always be able to connect, query device information,
    // install a Project and receive command responses.
    Transport::bleTransport().begin();
    Logger::info("V2 transport online");

    displayReady = Renderer::displayRenderer().begin();
    if (!displayReady) {
        Logger::error("Display initialization failed; BLE remains available");
    }

    // Loading configures only the active Scene. It must not refresh the panel.
    // Rendering is an explicit operation performed by the `apply` command.
    if (Project::projectManager().load()) {
        Logger::info("Stored Project ready; waiting for explicit apply");
    } else {
        Logger::info("No Project installed; waiting for Studio");
    }

    Logger::info("Display Studio 2.0 command runtime ready");
}

void Application::loop() {
    // V2 phase 1 deliberately does not call SceneRuntime::loop().
    // Automatic refresh previously allowed a 30-second e-ink operation to
    // starve BLE ACKs. Only an explicit `apply` command may render.
    Transport::bleTransport().loop();
    delay(2);
}
}
