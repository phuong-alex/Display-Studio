#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "display_studio/config.h"
#include "display_studio/core/logger.h"
#include "display_studio/device/device_identity.h"
#include "display_studio/project/project_manager.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/runtime/time_service.h"
#include "display_studio/transport/ble_transport.h"
#include "display_studio/version.h"

namespace DisplayStudio::Transport {
namespace {
BLECharacteristic* txCharacteristic = nullptr;
BLEServer* bleServer = nullptr;

bool clientConnected = false;
String receiveBuffer;
uint32_t rxFrameCount = 0;
uint16_t expectedRxSequence = 0;

// Keep the final frame ACK repeatable after a complete message.
// This fixes the Sprint 1.2.3 failure where a lost final ACK caused
// every retry of the final frame to be rejected as out-of-order.
bool completedAckValid = false;
uint16_t completedSequence = 0;

constexpr size_t BLE_SAFE_PAYLOAD = 20;
constexpr size_t BLE_FRAME_HEADER = 3;
constexpr uint8_t BLE_DATA_MARKER = 0xA5;
constexpr uint8_t BLE_ACK_MARKER = 0xA6;
constexpr uint32_t TX_CHUNK_INTERVAL_MS = 30;
constexpr size_t COMMAND_QUEUE_DEPTH = 3;
constexpr size_t RESPONSE_QUEUE_DEPTH = 8;
constexpr size_t ACK_QUEUE_DEPTH = 16;

QueueHandle_t commandQueue = nullptr;
QueueHandle_t responseQueue = nullptr;
QueueHandle_t ackQueue = nullptr;

char* activeResponse = nullptr;
size_t activeResponseLength = 0;
size_t activeResponseOffset = 0;
uint32_t lastTxAt = 0;

bool rebootPending = false;
uint32_t rebootRequestedAt = 0;

BleTransport instance;

void freeStringQueue(QueueHandle_t queue) {
    if (!queue) return;

    char* item = nullptr;
    while (xQueueReceive(queue, &item, 0) == pdTRUE) {
        free(item);
        item = nullptr;
    }
}

void clearTransportState() {
    receiveBuffer = "";
    rxFrameCount = 0;
    expectedRxSequence = 0;
    completedAckValid = false;
    completedSequence = 0;

    freeStringQueue(commandQueue);
    freeStringQueue(responseQueue);

    if (ackQueue) {
        xQueueReset(ackQueue);
    }

    if (activeResponse) {
        free(activeResponse);
        activeResponse = nullptr;
    }

    activeResponseLength = 0;
    activeResponseOffset = 0;
    rebootPending = false;
}

bool enqueueOwnedString(
    QueueHandle_t queue,
    const String& value
) {
    if (!queue) return false;

    char* copy = static_cast<char*>(
        malloc(value.length() + 1)
    );

    if (!copy) {
        Core::Logger::error(
            "BLE queue allocation failed"
        );
        return false;
    }

    memcpy(
        copy,
        value.c_str(),
        value.length() + 1
    );

    if (
        xQueueSend(queue, &copy, 0) !=
        pdTRUE
    ) {
        free(copy);
        Core::Logger::error(
            "BLE queue is full"
        );
        return false;
    }

    return true;
}

void enqueueTransportAck(
    uint16_t sequence
) {
    if (!ackQueue) return;

    if (
        xQueueSend(
            ackQueue,
            &sequence,
            0
        ) != pdTRUE
    ) {
        Core::Logger::warning(
            "BLE ACK queue full"
        );
    }
}

void enqueueJson(
    JsonDocument& document
) {
    String payload;
    serializeJson(document, payload);
    payload += "\n";

    if (enqueueOwnedString(responseQueue, payload)) {
        Core::Logger::info(
            "BLE response queued: " +
            String(payload.length()) +
            " bytes"
        );
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

    enqueueJson(response);
}

void sendDeviceInfo() {
    JsonDocument response;

    response["type"] = "device_info";
    response["deviceName"] =
        "Display Studio E-Ink";
    response["deviceId"] =
        Device::deviceIdentity().id();
    response["deviceFamily"] = "eink";
    response["firmware"] =
        Version::FIRMWARE;
    response["release"] =
        Version::RELEASE;
    response["protocolVersion"] =
        Version::PROTOCOL;
    response["configured"] =
        Project::projectManager().installed();
    response["projectModel"] =
        "display-studio/project-v1";
    response["timeReady"] =
        Runtime::timeService().ready();

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
    capabilities.add("ble-queued-runtime-v1");

    enqueueJson(response);
}

void sendStoredProject() {
    JsonDocument response;
    response["type"] = "stored_project";

    if (Project::projectManager().installed()) {
        response["found"] = true;
        response["project"].set(
            Project::projectManager().document()
        );
    } else {
        response["found"] = false;
    }

    enqueueJson(response);
}

void sendStoredConfigCompatibility() {
    JsonDocument response;
    response["type"] = "stored_config";

    const JsonVariantConst scene =
        Project::projectManager().activeScene();

    if (!scene.isNull()) {
        response["found"] = true;
        response["config"].set(
            scene["config"]
        );
    } else {
        response["found"] = false;
    }

    enqueueJson(response);
}

void requestReboot() {
    rebootPending = true;
    rebootRequestedAt = millis();
}

void handleLine(const String& line) {
    JsonDocument request;

    const DeserializationError error =
        deserializeJson(request, line);

    if (error) {
        Core::Logger::error(
            "BLE JSON parse failed: " +
            String(error.c_str())
        );
        sendStatus(false, "invalid_json");
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
        "BLE command executing: " + command +
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
            Runtime::timeService().setFromBrowser(
                request["epochMs"] | 0LL,
                request["timezoneOffsetMinutes"] |
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
            Project::projectManager().install(
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
            Project::projectManager().installLegacy(
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
            Project::projectManager().activateScene(
                sceneId
            );

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
            Runtime::sceneRuntime().renderNow();

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
        requestReboot();
        return;
    }

    if (command == "factory_reset") {
        Project::projectManager().clear();
        sendStatus(
            true,
            "factory_reset",
            requestId
        );
        requestReboot();
        return;
    }

    sendStatus(
        false,
        "unknown_command",
        requestId
    );
}

void pumpAck() {
    if (
        !clientConnected ||
        !txCharacteristic ||
        !ackQueue
    ) {
        return;
    }

    uint16_t sequence = 0;

    if (
        xQueueReceive(
            ackQueue,
            &sequence,
            0
        ) != pdTRUE
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
    lastTxAt = millis();
}

bool ackPending() {
    return ackQueue &&
        uxQueueMessagesWaiting(ackQueue) > 0;
}

void startNextResponse() {
    if (
        activeResponse ||
        !responseQueue
    ) {
        return;
    }

    char* next = nullptr;

    if (
        xQueueReceive(
            responseQueue,
            &next,
            0
        ) != pdTRUE
    ) {
        return;
    }

    activeResponse = next;
    activeResponseLength =
        strlen(activeResponse);
    activeResponseOffset = 0;

    Core::Logger::info(
        "BLE TX started: " +
        String(activeResponseLength) +
        " bytes"
    );
}

void pumpResponse() {
    if (
        !clientConnected ||
        !txCharacteristic ||
        ackPending()
    ) {
        return;
    }

    startNextResponse();

    if (!activeResponse) {
        return;
    }

    if (
        millis() - lastTxAt <
        TX_CHUNK_INTERVAL_MS
    ) {
        return;
    }

    const size_t length = min(
        BLE_SAFE_PAYLOAD,
        activeResponseLength -
            activeResponseOffset
    );

    uint8_t chunk[BLE_SAFE_PAYLOAD];

    memcpy(
        chunk,
        activeResponse +
            activeResponseOffset,
        length
    );

    txCharacteristic->setValue(
        chunk,
        length
    );
    txCharacteristic->notify();

    activeResponseOffset += length;
    lastTxAt = millis();

    if (
        activeResponseOffset >=
        activeResponseLength
    ) {
        Core::Logger::info(
            "BLE TX completed"
        );

        free(activeResponse);
        activeResponse = nullptr;
        activeResponseLength = 0;
        activeResponseOffset = 0;
    }
}

void executeNextCommand() {
    if (
        !commandQueue ||
        ackPending()
    ) {
        return;
    }

    char* command = nullptr;

    if (
        xQueueReceive(
            commandQueue,
            &command,
            0
        ) != pdTRUE
    ) {
        return;
    }

    String line(command);
    free(command);

    handleLine(line);
}

bool transportIdle() {
    return
        !activeResponse &&
        (!responseQueue ||
            uxQueueMessagesWaiting(
                responseQueue
            ) == 0) &&
        (!ackQueue ||
            uxQueueMessagesWaiting(
                ackQueue
            ) == 0);
}

class ServerCallbacks final
    : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        clientConnected = true;
        clearTransportState();

        Core::Logger::info(
            "BLE client connected"
        );
    }

    void onDisconnect(
        BLEServer* server
    ) override {
        clientConnected = false;
        clearTransportState();

        Core::Logger::info(
            "BLE client disconnected"
        );

        delay(100);
        server->getAdvertising()->start();

        Core::Logger::info(
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

        // A retry of the final frame can arrive after the message
        // state has reset to sequence zero. ACK it again without
        // appending it a second time.
        if (
            completedAckValid &&
            expectedRxSequence == 0 &&
            sequence == completedSequence
        ) {
            Core::Logger::warning(
                "BLE RX repeated final frame #" +
                String(sequence)
            );
            enqueueTransportAck(sequence);
            return;
        }

        if (sequence < expectedRxSequence) {
            Core::Logger::warning(
                "BLE RX duplicate frame #" +
                String(sequence)
            );
            enqueueTransportAck(sequence);
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

        if (
            sequence == 0 &&
            expectedRxSequence == 0
        ) {
            completedAckValid = false;
        }

        const size_t dataLength =
            value.size() - BLE_FRAME_HEADER;

        receiveBuffer.concat(
            reinterpret_cast<const char*>(
                bytes + BLE_FRAME_HEADER
            ),
            dataLength
        );

        rxFrameCount++;

        Core::Logger::info(
            "BLE RX frame #" +
            String(sequence) +
            ": " +
            String(dataLength) +
            " bytes, buffered=" +
            String(receiveBuffer.length())
        );

        enqueueTransportAck(sequence);
        expectedRxSequence++;

        const int newline =
            receiveBuffer.indexOf('\n');

        if (newline >= 0) {
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
                "BLE command queued: " +
                String(line.length()) +
                " bytes in " +
                String(rxFrameCount) +
                " frames"
            );

            if (!line.isEmpty()) {
                if (
                    !enqueueOwnedString(
                        commandQueue,
                        line
                    )
                ) {
                    Core::Logger::error(
                        "BLE command queue full"
                    );
                }
            }

            completedSequence = sequence;
            completedAckValid = true;
            rxFrameCount = 0;
            expectedRxSequence = 0;
        }

        if (
            receiveBuffer.length() >
            Config::MAX_PROJECT_BYTES + 2048
        ) {
            Core::Logger::error(
                "BLE RX buffer overflow"
            );

            receiveBuffer = "";
            rxFrameCount = 0;
            expectedRxSequence = 0;
            completedAckValid = false;

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
    commandQueue = xQueueCreate(
        COMMAND_QUEUE_DEPTH,
        sizeof(char*)
    );
    responseQueue = xQueueCreate(
        RESPONSE_QUEUE_DEPTH,
        sizeof(char*)
    );
    ackQueue = xQueueCreate(
        ACK_QUEUE_DEPTH,
        sizeof(uint16_t)
    );

    if (
        !commandQueue ||
        !responseQueue ||
        !ackQueue
    ) {
        Core::Logger::error(
            "BLE queue initialization failed"
        );
        return;
    }

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

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(
        new ServerCallbacks()
    );

    BLEService* service =
        bleServer->createService(
            Config::BLE_SERVICE_UUID
        );

    BLECharacteristic* rx =
        service->createCharacteristic(
            Config::BLE_RX_UUID,
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR
        );

    rx->setCallbacks(
        new RxCallbacks()
    );

    txCharacteristic =
        service->createCharacteristic(
            Config::BLE_TX_UUID,
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
        "BLE queued transport ready: " +
        String(Config::BLE_SERVICE_UUID)
    );
}

void BleTransport::loop() {
    if (!clientConnected) {
        return;
    }

    // One notification operation per loop iteration. ACKs always
    // take priority over JSON response chunks.
    if (ackPending()) {
        pumpAck();
        return;
    }

    executeNextCommand();
    pumpResponse();

    if (
        rebootPending &&
        transportIdle() &&
        millis() - rebootRequestedAt >
            500
    ) {
        ESP.restart();
    }
}

bool BleTransport::connected() const {
    return clientConnected;
}
}
