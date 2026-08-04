#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "app_config.h"
#include "config.h"
#include "device.h"
#include "display.h"
#include "image_job.h"
#include "logger.h"
#include "runtime_config.h"
#include "wifi_manager.h"

namespace {
uint32_t lastCheckAt = 0;
bool firstCheck = true;

bool downloadBinary(const String& url, uint8_t* buffer, size_t expectedBytes) {
    HTTPClient http;
    http.setTimeout(Config::HTTP_TIMEOUT_MS);
    http.begin(url);

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        Logger::warning("Image GET failed: HTTP " + String(status));
        http.end();
        return false;
    }

    const int contentLength = http.getSize();
    Logger::info(
        "Image content length: " + String(contentLength) +
        ", expected: " + String(expectedBytes)
    );

    if (contentLength > 0 &&
        contentLength != static_cast<int>(expectedBytes)) {
        Logger::warning("Invalid image size");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t received = 0;
    uint32_t lastDataAt = millis();

    while (http.connected() &&
           received < expectedBytes &&
           millis() - lastDataAt < Config::HTTP_TIMEOUT_MS) {
        const size_t available = stream->available();

        if (available > 0) {
            const size_t remaining = expectedBytes - received;
            const size_t toRead = min(available, remaining);

            const size_t count = stream->readBytes(
                buffer + received,
                toRead
            );

            if (count > 0) {
                received += count;
                lastDataAt = millis();
            }
        } else {
            delay(2);
        }
    }

    http.end();

    Logger::info(
        "Image bytes received: " + String(received) +
        "/" + String(expectedBytes)
    );

    return received == expectedBytes;
}

void reportDone(
    const String& jobId,
    const bool ok,
    const String& message
) {
    JsonDocument doc;
    doc["device_id"] = Device::id();
    doc["job_id"] = jobId;
    doc["ok"] = ok;
    doc["message"] = message;

    String payload;
    serializeJson(doc, payload);

    HTTPClient http;
    http.setTimeout(Config::HTTP_TIMEOUT_MS);
    http.begin(AppConfig::serverUrl() + "/api/device/job-done");
    http.addHeader("Content-Type", "application/json");

    const int status = http.POST(payload);
    Logger::info(
        "Job report: " + message +
        ", HTTP " + String(status)
    );

    http.end();
}
}

namespace ImageJob {
void begin() {
    firstCheck = true;
}

void checkNow() {
    if (!WiFiManager::isConnected()) {
        return;
    }

    const String url =
        AppConfig::serverUrl() +
        "/api/device/job?device_id=" +
        Device::id();

    HTTPClient http;
    http.setTimeout(Config::HTTP_TIMEOUT_MS);
    http.begin(url);

    const int status = http.GET();

    if (status != HTTP_CODE_OK) {
        Logger::warning("Job check failed: HTTP " + String(status));
        http.end();
        return;
    }

    JsonDocument doc;
    const DeserializationError error =
        deserializeJson(doc, http.getString());

    http.end();

    if (error) {
        Logger::warning("Invalid job JSON");
        return;
    }

    const bool update = doc["update"] | false;

    if (!update) {
        lastCheckAt = millis();
        firstCheck = false;
        return;
    }

    const String jobId = doc["job_id"] | "";
    const String imageUrl = doc["image_url"] | "";

    if (jobId.isEmpty() || imageUrl.isEmpty()) {
        Logger::warning("Incomplete image job");
        return;
    }

    if (jobId == AppConfig::lastJobId()) {
        reportDone(jobId, true, "already_applied");
        lastCheckAt = millis();
        firstCheck = false;
        return;
    }

    Logger::info("New image job: " + jobId);
    Logger::info("Image URL: " + imageUrl);
    Logger::info("Required image bytes: " + String(Config::IMAGE_BYTES));
    Logger::info("Free heap before allocation: " + String(ESP.getFreeHeap()));
    Logger::info("Free PSRAM: " + String(ESP.getFreePsram()));

    // Ảnh chỉ khoảng 7.8 KB, vì vậy dùng heap thường.
    // Không phụ thuộc việc PSRAM đã được PlatformIO bật hay chưa.
    uint8_t* imageBuffer =
        static_cast<uint8_t*>(malloc(Config::IMAGE_BYTES));

    if (!imageBuffer) {
        Logger::error("Image buffer allocation failed");
        reportDone(jobId, false, "memory_allocation_failed");
        lastCheckAt = millis();
        firstCheck = false;
        return;
    }

    if (!downloadBinary(
            imageUrl,
            imageBuffer,
            Config::IMAGE_BYTES
        )) {
        free(imageBuffer);
        reportDone(jobId, false, "download_failed");
        lastCheckAt = millis();
        firstCheck = false;
        return;
    }

    const uint8_t* blackPlane = imageBuffer;
    const uint8_t* redPlane =
        imageBuffer + Config::PLANE_BYTES;

    Logger::info("Refreshing e-ink display...");

    const bool displayed = Display::showRawImage(
        blackPlane,
        redPlane,
        Config::PLANE_BYTES
    );

    free(imageBuffer);

    if (displayed) {
        AppConfig::setLastJobId(jobId);
        reportDone(jobId, true, "display_updated");
        Logger::info("Display updated successfully");
    } else {
        reportDone(jobId, false, "display_failed");
        Logger::error("Display update failed");
    }

    lastCheckAt = millis();
    firstCheck = false;
}

void loop() {
    if (!WiFiManager::isConnected()) {
        return;
    }

    if (firstCheck ||
        millis() - lastCheckAt >=
            RuntimeConfig::jobIntervalMs()) {
        checkNow();
    }
}
}
