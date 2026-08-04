#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include "display_studio/config.h"
#include "display_studio/core/logger.h"
#include "display_studio/device/device_identity.h"
#include "display_studio/project/project_manager.h"
#include "display_studio/project/project_model.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/runtime/time_service.h"
#include "display_studio/storage/project_storage.h"
#include "display_studio/transport/ble_transport.h"
#include "display_studio/version.h"

namespace DisplayStudio::Transport {
namespace {
BLECharacteristic* txCharacteristic =
    nullptr;

bool clientConnected = false;
String receiveBuffer;
uint32_t rxChunkCount = 0;
BleTransport instance;

constexpr size_t BLE_SAFE_PAYLOAD = 20;
constexpr size_t BLE_FRAME_HEADER = 3;
constexpr size_t BLE_FRAME_DATA =
    BLE_SAFE_PAYLOAD - BLE_FRAME_HEADER;
constexpr uint8_t BLE_DATA_MARKER = 0xA5;
constexpr uint8_t BLE_ACK_MARKER = 0xA6;
constexpr uint32_t TX_CHUNK_DELAY_MS = 18;
uint16_t expectedRxSequence = 0;

void sendTransportAck(
    uint16_t sequence
) {
    if (
        !clientConnected ||
        !txCharacteristic
    ) {
        return;
    }

    uint8_t ack[3] = {
        BLE_ACK_MARKER,
        static_cast<uint8_t>(
            sequence & 0xFF
        ),
        static_cast<uint8_t>(
            (sequence >> 8) & 0xFF
        )
    };

    txCharacteristic->setValue(
        ack,
        sizeof(ack)
    );

    txCharacteristic->notify();
}

void notifyJson(
    JsonDocument& document
) {
    if (
        !clientConnected ||
        !txCharacteristic
    ) {
        return;
    }

    String payload;
    serializeJson(document, payload);
    payload += "\n";

    const size_t total = payload.length();
    const size_t chunks =
        (total + BLE_SAFE_PAYLOAD - 1) /
        BLE_SAFE_PAYLOAD;

    Core::Logger::info(
        "BLE TX message: " +
        String(total) +
        " bytes, " +
        String(chunks) +
        " chunks"
    );

    for (
        size_t offset = 0;
        offset < total;
        offset += BLE_SAFE_PAYLOAD
    ) {
        const size_t length = min(
            BLE_SAFE_PAYLOAD,
            total - offset
        );

        uint8_t chunk[BLE_SAFE_PAYLOAD];

        memcpy(
            chunk,
            payload.c_str() + offset,
            length
        );

        txCharacteristic->setValue(
            chunk,
            length
        );

        txCharacteristic->notify();
        delay(TX_CHUNK_DELAY_MS);
    }
}

void sendStatus(
    bool ok,
    const String& message,
    const String& requestId = ""
) {
    JsonDocument response;
    response["type"] = "status";
    response["ok"] = ok;
    response["message"] = message;

    if (!requestId.isEmpty()) {
        response["requestId"] = requestId;
    }

    notifyJson(response);
}

void sendDeviceInfo() {
    JsonDocument response;

    response["type"] =
        "device_info";

    response["deviceName"] =
        "Display Studio E-Ink";

    response["deviceId"] =
        Device::deviceIdentity().id();

    response["deviceFamily"] =
        "eink";

    response["firmware"] =
        Version::FIRMWARE;

    response["release"] =
        Version::RELEASE;

    response["protocolVersion"] =
        Version::PROTOCOL;

    response["configured"] =
        Project::projectManager()
            .installed();

    response["projectModel"] =
        "display-studio/project-v1";

    response["timeReady"] =
        Runtime::timeService()
            .ready();

    response["display"]["adapter"] =
        "gxepd2-213-z98c";

    response["display"]["width"] =
        Config::SCREEN_WIDTH;

    response["display"]["height"] =
        Config::SCREEN_HEIGHT;

    response["display"]["colorMode"] =
        "bwr";

    response["display"]
        ["partialRefresh"] = false;

    JsonArray capabilities =
        response["capabilities"]
            .to<JsonArray>();

    capabilities.add("device-info");
    capabilities.add("persistent-project");
    capabilities.add("multiple-scenes");
    capabilities.add("scene-activation");
    capabilities.add("legacy-config-migration");
    capabilities.add("browser-time");
    capabilities.add("clock");
    capabilities.add("calendar");
    capabilities.add("reboot");
    capabilities.add("factory-reset");
    capabilities.add("ble-stop-and-wait-v1");

    notifyJson(response);
}

void sendStoredProject() {
    JsonDocument response;
    response["type"] = "stored_project";

    if (
        Project::projectManager()
            .installed()
    ) {
        response["found"] = true;
        response["project"].set(
            Project::projectManager()
                .document()
        );
    } else {
        response["found"] = false;
    }

    notifyJson(response);
}

void sendStoredConfigCompatibility() {
    JsonDocument response;
    response["type"] = "stored_config";

    const JsonVariantConst scene =
        Project::projectManager()
            .activeScene();

    if (!scene.isNull()) {
        response["found"] = true;
        response["config"].set(
            scene["config"]
        );
    } else {
        response["found"] = false;
    }

    notifyJson(response);
}

void handleLine(
    const String& line
) {
    JsonDocument request;

    const DeserializationError error =
        deserializeJson(
            request,
            line
        );

    if (error) {
        Core::Logger::error(
            "BLE JSON parse failed: " +
            String(error.c_str())
        );

        sendStatus(
            false,
            "invalid_json"
        );
        return;
    }

    const String command = String(
        static_cast<const char*>(
            request["command"] | ""
        )
    );

    const String requestId = String(
        static_cast<const char*>(
            request["requestId"] | ""
        )
    );

    Core::Logger::info(
        "BLE command received: " + command +
        (
            requestId.isEmpty()
                ? ""
                : " [" + requestId + "]"
        )
    );

    if (command == "get_info") {
        sendDeviceInfo();
        return;
    }

    if (command == "get_project") {
        sendStoredProject();
        return;
    }

    if (command == "get_config") {
        sendStoredConfigCompatibility();
        return;
    }

    if (command == "ping") {
        sendStatus(true, "pong", requestId);
        return;
    }

    if (command == "set_time") {
        const bool ok =
            Runtime::timeService()
                .setFromBrowser(
                    request["epochMs"] |
                        0LL,
                    request
                        ["timezoneOffsetMinutes"] |
                        420
                );

        sendStatus(
            ok,
            ok
                ? "time_synchronized"
                : "time_sync_failed",
            requestId
        );

        return;
    }

    if (command == "set_project") {
        const bool installed =
            Project::projectManager()
                .install(
                    request["project"]
                );

        sendStatus(
            installed,
            installed
                ? "project_installed"
                : "project_install_failed",
            requestId
        );

        return;
    }

    if (command == "set_config") {
        const bool installed =
            Project::projectManager()
                .installLegacy(
                    request["config"]
                );

        sendStatus(
            installed,
            installed
                ? "legacy_config_migrated"
                : "config_save_failed",
            requestId
        );

        return;
    }

    if (command == "activate_scene") {
        const String sceneId = String(
            static_cast<const char*>(
                request["sceneId"] | ""
            )
        );

        const bool activated =
            Project::projectManager()
                .activateScene(sceneId);

        sendStatus(
            activated,
            activated
                ? "scene_activated"
                : "scene_activation_failed",
            requestId
        );

        return;
    }

    if (command == "apply") {
        const bool rendered =
            Runtime::sceneRuntime()
                .renderNow();

        sendStatus(
            rendered,
            rendered
                ? "scene_rendered"
                : "scene_render_failed",
            requestId
        );

        return;
    }

    if (command == "reboot") {
        sendStatus(true, "rebooting", requestId);
        delay(300);
        ESP.restart();
        return;
    }

    if (command == "factory_reset") {
        Project::projectManager()
            .clear();

        sendStatus(
            true,
            "factory_reset",
            requestId
        );

        delay(300);
        ESP.restart();
        return;
    }

    sendStatus(
        false,
        "unknown_command",
        requestId
    );
}

class ServerCallbacks final
    : public BLEServerCallbacks {
    void onConnect(
        BLEServer*
    ) override {
        clientConnected = true;
        receiveBuffer = "";
        rxChunkCount = 0;
        expectedRxSequence = 0;

        Core::Logger::info(
            "BLE client connected"
        );
    }

    void onDisconnect(
        BLEServer* server
    ) override {
        clientConnected = false;
        receiveBuffer = "";
        rxChunkCount = 0;
        expectedRxSequence = 0;

        Core::Logger::info(
            "BLE client disconnected"
        );

        delay(100);
        server->getAdvertising()
            ->start();

        Core::Logger::info(
            "BLE advertising restarted"
        );
    }
};

class RxCallbacks final
    : public BLECharacteristicCallbacks {
    void onWrite(
        BLECharacteristic*
            characteristic
    ) override {
        const std::string value =
            characteristic->getValue();

        if (value.empty()) {
            return;
        }

        const uint8_t* bytes =
            reinterpret_cast<const uint8_t*>(
                value.data()
            );

        if (
            value.size() < BLE_FRAME_HEADER ||
            bytes[0] != BLE_DATA_MARKER
        ) {
            Core::Logger::warning(
                "BLE RX invalid frame"
            );
            return;
        }

        const uint16_t sequence =
            static_cast<uint16_t>(bytes[1]) |
            (
                static_cast<uint16_t>(bytes[2])
                << 8
            );

        if (sequence < expectedRxSequence) {
            Core::Logger::warning(
                "BLE RX duplicate frame #" +
                String(sequence)
            );
            sendTransportAck(sequence);
            return;
        }

        if (sequence > expectedRxSequence) {
            Core::Logger::warning(
                "BLE RX out-of-order frame #" +
                String(sequence) +
                ", expected=" +
                String(expectedRxSequence)
            );
            return;
        }

        const size_t dataLength =
            value.size() - BLE_FRAME_HEADER;

        receiveBuffer.concat(
            reinterpret_cast<const char*>(
                bytes + BLE_FRAME_HEADER
            ),
            dataLength
        );

        rxChunkCount++;

        Core::Logger::info(
            "BLE RX frame #" +
            String(sequence) +
            ": " +
            String(dataLength) +
            " bytes, buffered=" +
            String(receiveBuffer.length())
        );

        // ACK immediately. Command execution may take several
        // seconds, especially when the e-paper is refreshed.
        sendTransportAck(sequence);
        expectedRxSequence++;

        while (
            receiveBuffer.indexOf('\n') >= 0
        ) {
            const int newline =
                receiveBuffer.indexOf('\n');

            String line =
                receiveBuffer.substring(
                    0,
                    newline
                );

            receiveBuffer.remove(
                0,
                newline + 1
            );

            line.trim();

            Core::Logger::info(
                "BLE RX message complete: " +
                String(line.length()) +
                " bytes in " +
                String(rxChunkCount) +
                " frames"
            );

            rxChunkCount = 0;
            expectedRxSequence = 0;

            if (!line.isEmpty()) {
                handleLine(line);
            }
        }

        if (
            receiveBuffer.length() >
            Config::MAX_PROJECT_BYTES + 2048
        ) {
            Core::Logger::error(
                "BLE RX buffer overflow"
            );

            receiveBuffer = "";
            rxChunkCount = 0;
            expectedRxSequence = 0;

            sendStatus(
                false,
                "rx_overflow"
            );
        }
    }
};
}

BleTransport& bleTransport() {
    return instance;
}

void BleTransport::begin() {
    String suffix =
        Device::deviceIdentity().id();

    if (suffix.length() > 4) {
        suffix = suffix.substring(
            suffix.length() - 4
        );
    }

    const String advertisedName =
        "DisplayStudio-" + suffix;

    BLEDevice::init(
        advertisedName.c_str()
    );

    BLEDevice::setMTU(185);

    BLEServer* server =
        BLEDevice::createServer();

    server->setCallbacks(
        new ServerCallbacks()
    );

    BLEService* service =
        server->createService(
            Config::BLE_SERVICE_UUID
        );

    BLECharacteristic* rx =
        service->createCharacteristic(
            Config::BLE_RX_UUID,
            BLECharacteristic::
                PROPERTY_WRITE |
            BLECharacteristic::
                PROPERTY_WRITE_NR
        );

    rx->setCallbacks(
        new RxCallbacks()
    );

    txCharacteristic =
        service->createCharacteristic(
            Config::BLE_TX_UUID,
            BLECharacteristic::
                PROPERTY_READ |
            BLECharacteristic::
                PROPERTY_NOTIFY
        );

    txCharacteristic->addDescriptor(
        new BLE2902()
    );

    service->start();

    BLEAdvertising* advertising =
        BLEDevice::getAdvertising();

    advertising->addServiceUUID(
        Config::BLE_SERVICE_UUID
    );

    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);
    advertising->start();

    Core::Logger::info(
        "BLE advertising: " +
        advertisedName
    );

    Core::Logger::info(
        "BLE service ready: " +
        String(
            Config::BLE_SERVICE_UUID
        )
    );
}

void BleTransport::loop() {
}

bool BleTransport::connected() const {
    return clientConnected;
}
}
