#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>

#include "app_config.h"
#include "device.h"
#include "logger.h"
#include "ota_update.h"
#include "runtime_config.h"
#include "wifi_manager.h"

namespace {
uint32_t lastCheckAt = 0;
bool firstCheck = false;

void reportStatus(
    const String& status,
    const String& message
) {
    JsonDocument doc;
    doc["device_id"] = Device::id();
    doc["status"] = status;
    doc["message"] = message;

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.setTimeout(15000);
    http.begin(
        AppConfig::serverUrl() +
        "/api/device/firmware/status"
    );
    http.addHeader("Content-Type", "application/json");
    http.POST(body);
    http.end();
}

void checkNow() {
    if (!WiFiManager::isConnected()) {
        return;
    }

    HTTPClient check;
    check.setTimeout(15000);
    check.begin(
        AppConfig::serverUrl() +
        "/api/device/firmware?device_id=" +
        Device::id()
    );

    const int status = check.GET();

    if (status == HTTP_CODE_NO_CONTENT) {
        check.end();
        lastCheckAt = millis();
        firstCheck = false;
        return;
    }

    if (status != HTTP_CODE_OK) {
        Logger::warning(
            "OTA check HTTP " + String(status)
        );
        check.end();
        lastCheckAt = millis();
        firstCheck = false;
        return;
    }

    JsonDocument doc;
    const DeserializationError error =
        deserializeJson(doc, check.getString());

    check.end();

    if (error) {
        Logger::warning("Invalid OTA JSON");
        return;
    }

    if (!(doc["compatible"] | false)) {
        Logger::warning("OTA package is incompatible");
        reportStatus("failed", "board_mismatch");
        lastCheckAt = millis();
        firstCheck = false;
        return;
    }

    const String path = String(
        static_cast<const char*>(
            doc["assignment"]["download_url"] | ""
        )
    );

    const size_t expectedSize =
        doc["assignment"]["size"] | 0;

    if (path.isEmpty() || expectedSize == 0) {
        Logger::warning("Incomplete OTA assignment");
        return;
    }

    const String downloadUrl =
        path.startsWith("http")
            ? path
            : AppConfig::serverUrl() + path;

    Logger::info(
        "Starting OTA, bytes: " +
        String(expectedSize)
    );

    reportStatus("downloading", "download_started");

    WiFiClient client;
    HTTPClient download;
    download.setTimeout(30000);

    if (!download.begin(client, downloadUrl)) {
        reportStatus("failed", "download_begin_failed");
        return;
    }

    const int downloadStatus = download.GET();

    if (downloadStatus != HTTP_CODE_OK) {
        reportStatus(
            "failed",
            "download_http_" + String(downloadStatus)
        );
        download.end();
        return;
    }

    if (!Update.begin(expectedSize)) {
        reportStatus(
            "failed",
            "update_begin_failed"
        );
        download.end();
        return;
    }

    reportStatus("installing", "writing_flash");

    const size_t written =
        Update.writeStream(
            *download.getStreamPtr()
        );

    const bool success =
        written == expectedSize &&
        Update.end() &&
        Update.isFinished();

    download.end();

    if (!success) {
        reportStatus(
            "failed",
            "update_failed_" +
            String(Update.getError())
        );
        return;
    }

    reportStatus("done", "ota_completed");
    delay(500);

    const uint32_t sleepSeconds =
        RuntimeConfig::sleepAfterUpdateSeconds();

    if (sleepSeconds > 0) {
        esp_sleep_enable_timer_wakeup(
            static_cast<uint64_t>(sleepSeconds) *
            1000000ULL
        );
        esp_deep_sleep_start();
    }

    ESP.restart();
}
}

namespace OtaUpdate {
void begin() {
    // Do not check immediately after every USB flash.
    // First OTA check occurs after the configured interval.
    firstCheck = false;
    lastCheckAt = millis();
}

void loop() {
    if (!WiFiManager::isConnected()) {
        return;
    }

    if (
        firstCheck ||
        millis() - lastCheckAt >=
            RuntimeConfig::otaIntervalMs()
    ) {
        checkNow();
    }
}
}
