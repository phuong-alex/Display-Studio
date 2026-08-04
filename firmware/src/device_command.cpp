#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "app_config.h"
#include "device.h"
#include "device_command.h"
#include "display.h"
#include "logger.h"
#include "native_app.h"
#include "runtime_config.h"
#include "wifi_manager.h"

namespace {
uint32_t lastCheckAt = 0;
bool firstCheck = true;

void reportDone(
    const String& commandId,
    bool ok,
    const String& message
) {
    JsonDocument doc;
    doc["device_id"] = Device::id();
    doc["command_id"] = commandId;
    doc["ok"] = ok;
    doc["message"] = message;

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.setTimeout(15000);
    http.begin(
        AppConfig::serverUrl() +
        "/api/device/command-done"
    );
    http.addHeader("Content-Type", "application/json");

    const int status = http.POST(body);
    Logger::info(
        "Command report HTTP " + String(status)
    );
    http.end();
}

void execute(
    const String& commandId,
    const String& type,
    JsonVariantConst payload
) {
    if (type == "refresh") {
        Display::refresh();
        reportDone(commandId, true, "display_refreshed");
        return;
    }

    if (type == "clear") {
        Display::clear();
        reportDone(commandId, true, "display_cleared");
        return;
    }

    if (type == "native-clock") {
        reportDone(
            commandId,
            NativeApp::showClock(payload),
            "native_clock_rendered"
        );
        return;
    }

    if (type == "native-calendar") {
        reportDone(
            commandId,
            NativeApp::showCalendar(payload),
            "native_calendar_rendered"
        );
        return;
    }

    if (type == "restart") {
        reportDone(commandId, true, "restarting");
        delay(500);
        ESP.restart();
        return;
    }

    if (type == "sleep") {
        const uint32_t minutes =
            payload["minutes"] | 5;

        reportDone(commandId, true, "deep_sleep");
        delay(300);

        esp_sleep_enable_timer_wakeup(
            static_cast<uint64_t>(minutes) *
            60ULL *
            1000000ULL
        );

        esp_deep_sleep_start();
        return;
    }

    reportDone(
        commandId,
        false,
        "unsupported_command"
    );
}

void checkNow() {
    if (!WiFiManager::isConnected()) {
        return;
    }

    HTTPClient http;
    http.setTimeout(15000);
    http.begin(
        AppConfig::serverUrl() +
        "/api/device/command?device_id=" +
        Device::id()
    );

    const int status = http.GET();

    if (status == HTTP_CODE_NO_CONTENT) {
        http.end();
        lastCheckAt = millis();
        firstCheck = false;
        return;
    }

    if (status != HTTP_CODE_OK) {
        Logger::warning(
            "Command check HTTP " + String(status)
        );
        http.end();
        lastCheckAt = millis();
        firstCheck = false;
        return;
    }

    JsonDocument doc;
    const DeserializationError error =
        deserializeJson(doc, http.getString());

    http.end();

    if (error) {
        Logger::warning("Invalid command JSON");
        return;
    }

    const String commandId = String(
        static_cast<const char*>(
            doc["command"]["id"] | ""
        )
    );

    const String type = String(
        static_cast<const char*>(
            doc["command"]["type"] | ""
        )
    );

    if (commandId.isEmpty() || type.isEmpty()) {
        Logger::warning("Incomplete command");
        return;
    }

    Logger::info("Executing command: " + type);

    execute(
        commandId,
        type,
        doc["command"]["payload"]
    );

    lastCheckAt = millis();
    firstCheck = false;
}
}

namespace DeviceCommand {
void begin() {
    firstCheck = true;
}

void loop() {
    if (!WiFiManager::isConnected()) {
        return;
    }

    if (
        firstCheck ||
        millis() - lastCheckAt >=
            RuntimeConfig::commandIntervalMs()
    ) {
        checkNow();
    }
}
}
