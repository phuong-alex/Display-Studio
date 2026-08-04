#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "app_config.h"
#include "config.h"
#include "device.h"
#include "heartbeat.h"
#include "logger.h"
#include "runtime_config.h"
#include "version.h"
#include "wifi_manager.h"

namespace {
uint32_t lastHeartbeatAt = 0;
bool firstHeartbeat = true;
}

namespace Heartbeat {
void begin() { firstHeartbeat = true; }

void sendNow() {
    if (!WiFiManager::isConnected()) return;

    JsonDocument doc;
    doc["device_id"] = Device::id();
    doc["name"] = Device::name();
    doc["mac"] = Device::macAddress();
    doc["ip"] = WiFiManager::ipAddress();
    doc["firmware"] = Version::FIRMWARE;
    doc["screen"] = "GxEPD2_213_Z98c";
    doc["width"] = Config::SCREEN_WIDTH;
    doc["height"] = Config::SCREEN_HEIGHT;
    doc["rssi"] = WiFiManager::rssi();
    doc["free_heap"] = Device::freeHeap();
    doc["free_psram"] = Device::freePsram();
    doc["uptime_ms"] = millis();
    doc["configuration_version"] =
        RuntimeConfig::version();

    JsonObject capabilities =
        doc["capabilities"].to<JsonObject>();

    capabilities["board"] =
        "esp32-s3-devkitc-1";
    capabilities["firmware_version"] =
        Version::FIRMWARE;

    JsonObject displayInfo =
        capabilities["display"].to<JsonObject>();

    displayInfo["driver"] =
        "GxEPD2_213_Z98c";
    displayInfo["width"] =
        Config::SCREEN_WIDTH;
    displayInfo["height"] =
        Config::SCREEN_HEIGHT;
    displayInfo["color_mode"] =
        "black-red-white";

    JsonObject features =
        capabilities["features"].to<JsonObject>();

    features["ota"] = true;
    features["native_commands"] = true;
    features["native_clock"] = true;
    features["native_calendar"] = true;

    String payload;
    serializeJson(doc, payload);

    HTTPClient http;
    http.setTimeout(Config::HTTP_TIMEOUT_MS);
    http.begin(AppConfig::serverUrl() + "/api/device/heartbeat");
    http.addHeader("Content-Type", "application/json");

    const int status = http.POST(payload);

    if (status >= 200 && status < 300) {
        Logger::info("Heartbeat OK");

        JsonDocument response;
        const DeserializationError error =
            deserializeJson(
                response,
                http.getString()
            );

        if (!error &&
            RuntimeConfig::apply(
                response["configuration"]
            )) {
            Logger::info(
                "Runtime configuration applied: v" +
                String(RuntimeConfig::version())
            );

            JsonDocument ack;
            ack["device_id"] = Device::id();
            ack["version"] =
                RuntimeConfig::version();

            String ackBody;
            serializeJson(ack, ackBody);

            HTTPClient ackHttp;
            ackHttp.setTimeout(
                Config::HTTP_TIMEOUT_MS
            );
            ackHttp.begin(
                AppConfig::serverUrl() +
                "/api/device/configuration-applied"
            );
            ackHttp.addHeader(
                "Content-Type",
                "application/json"
            );
            ackHttp.POST(ackBody);
            ackHttp.end();
        }
    } else {
        Logger::warning(
            "Heartbeat failed: " +
            String(status)
        );
    }

    http.end();

    lastHeartbeatAt = millis();
    firstHeartbeat = false;
}

void loop() {
    if (!WiFiManager::isConnected()) return;
    if (firstHeartbeat || millis() - lastHeartbeatAt >= RuntimeConfig::heartbeatIntervalMs()) {
        sendNow();
    }
}
}
