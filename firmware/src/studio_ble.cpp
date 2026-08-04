#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include "config.h"
#include "device.h"
#include "logger.h"
#include "studio_ble.h"
#include "studio_config.h"
#include "studio_runtime.h"
#include "studio_time.h"
#include "version.h"

namespace {
constexpr const char* SERVICE_UUID =
    "7fb90001-6f3e-4e65-9c7a-f13827b6a001";
constexpr const char* RX_UUID =
    "7fb90002-6f3e-4e65-9c7a-f13827b6a001";
constexpr const char* TX_UUID =
    "7fb90003-6f3e-4e65-9c7a-f13827b6a001";

BLECharacteristic* txCharacteristic = nullptr;
bool clientConnected = false;
String receiveBuffer;

void notifyJson(JsonDocument& document) {
    if (!clientConnected || !txCharacteristic) {
        return;
    }

    String payload;
    serializeJson(document, payload);
    payload += "\n";

    txCharacteristic->setValue(payload.c_str());
    txCharacteristic->notify();
}

void sendStatus(
    bool ok,
    const String& message
) {
    JsonDocument response;
    response["type"] = "status";
    response["ok"] = ok;
    response["message"] = message;
    notifyJson(response);
}

void sendDeviceInfo() {
    JsonDocument response;
    response["type"] = "device_info";
    response["deviceName"] =
        "Display Studio E-Ink";
    response["deviceId"] = Device::id();
    response["deviceFamily"] = "eink";
    response["firmware"] = Version::FIRMWARE;
    response["protocolVersion"] =
        Version::PROTOCOL;
    response["configured"] =
        StudioRuntime::configured();
    response["timeReady"] =
        StudioTime::ready();

    response["display"]["adapter"] =
        "gxepd2-213-z98c";
    response["display"]["width"] =
        Config::SCREEN_WIDTH;
    response["display"]["height"] =
        Config::SCREEN_HEIGHT;
    response["display"]["colorMode"] =
        "bwr";
    response["display"]["partialRefresh"] =
        false;

    JsonArray capabilities =
        response["capabilities"].to<JsonArray>();

    capabilities.add("device-info");
    capabilities.add("persistent-scene");
    capabilities.add("browser-time");
    capabilities.add("clock");
    capabilities.add("calendar");
    capabilities.add("reboot");
    capabilities.add("factory-reset");
    capabilities.add("image-engine-beta13");

    notifyJson(response);
}

void sendStoredConfig() {
    JsonDocument stored;
    JsonDocument response;
    response["type"] = "stored_config";

    if (StudioConfig::load(stored)) {
        response["found"] = true;
        response["config"].set(
            stored.as<JsonVariantConst>()
        );
    } else {
        response["found"] = false;
    }

    notifyJson(response);
}

void handleLine(const String& line) {
    JsonDocument request;
    const DeserializationError error =
        deserializeJson(request, line);

    if (error) {
        sendStatus(false, "invalid_json");
        return;
    }

    const String command = String(
        static_cast<const char*>(
            request["command"] | ""
        )
    );

    if (command == "get_info") {
        sendDeviceInfo();
        return;
    }

    if (command == "get_config") {
        sendStoredConfig();
        return;
    }

    if (command == "ping") {
        sendStatus(true, "pong");
        return;
    }

    if (command == "set_time") {
        const bool ok =
            StudioTime::setFromBrowser(
                request["epochMs"] | 0LL,
                request["timezoneOffsetMinutes"] |
                    420
            );

        sendStatus(
            ok,
            ok
                ? "time_synchronized"
                : "time_sync_failed"
        );
        return;
    }

    if (command == "set_config") {
        const JsonVariantConst config =
            request["config"];

        const bool valid =
            StudioRuntime::configure(config);

        const bool saved =
            valid && StudioConfig::save(config);

        sendStatus(
            saved,
            saved
                ? "config_saved"
                : "config_save_failed"
        );
        return;
    }

    if (command == "apply") {
        const bool rendered =
            StudioRuntime::renderNow();

        sendStatus(
            rendered,
            rendered
                ? "scene_rendered"
                : "scene_render_failed"
        );
        return;
    }

    if (command == "reboot") {
        sendStatus(true, "rebooting");
        delay(300);
        ESP.restart();
        return;
    }

    if (command == "factory_reset") {
        StudioConfig::clear();
        StudioRuntime::clear();
        sendStatus(true, "factory_reset");
        delay(300);
        ESP.restart();
        return;
    }

    sendStatus(false, "unknown_command");
}

class ServerCallbacks final
    : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        clientConnected = true;
        Logger::info("BLE client connected");
    }

    void onDisconnect(
        BLEServer* server
    ) override {
        clientConnected = false;
        receiveBuffer = "";

        Logger::info(
            "BLE client disconnected"
        );

        delay(100);
        server->getAdvertising()->start();

        Logger::info(
            "BLE advertising restarted"
        );
    }
};

class RxCallbacks final
    : public BLECharacteristicCallbacks {
    void onWrite(
        BLECharacteristic* characteristic
    ) override {
        const std::string value =
            characteristic->getValue();

        if (value.empty()) {
            return;
        }

        receiveBuffer +=
            String(value.c_str());

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

            if (!line.isEmpty()) {
                handleLine(line);
            }
        }

        if (receiveBuffer.length() > 12000) {
            receiveBuffer = "";
            sendStatus(false, "rx_overflow");
        }
    }
};
}

namespace StudioBle {
void begin() {
    String suffix = Device::id();

    if (suffix.length() > 4) {
        suffix = suffix.substring(
            suffix.length() - 4
        );
    }

    const String advertisedName =
        "DisplayStudio-" + suffix;

    BLEDevice::init(advertisedName.c_str());
    BLEDevice::setMTU(185);

    BLEServer* server =
        BLEDevice::createServer();

    server->setCallbacks(
        new ServerCallbacks()
    );

    BLEService* service =
        server->createService(SERVICE_UUID);

    BLECharacteristic* rx =
        service->createCharacteristic(
            RX_UUID,
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR
        );

    rx->setCallbacks(new RxCallbacks());

    txCharacteristic =
        service->createCharacteristic(
            TX_UUID,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY
        );

    txCharacteristic->addDescriptor(
        new BLE2902()
    );

    service->start();

    BLEAdvertising* advertising =
        BLEDevice::getAdvertising();

    advertising->addServiceUUID(
        SERVICE_UUID
    );

    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);
    advertising->start();

    Logger::info(
        "BLE advertising: " +
        advertisedName
    );

    Logger::info(
        "BLE service ready: " +
        String(SERVICE_UUID)
    );
}

void loop() {
}

bool connected() {
    return clientConnected;
}
}
