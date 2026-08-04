#include "OtaClient.h"

#include <ArduinoJson.h>

bool OtaClient::reportStatus(
    const String& status,
    const String& message) {
  HTTPClient http;
  http.begin(serverUrl_ + "/api/device/firmware/status");
  http.addHeader("Content-Type", "application/json");

  JsonDocument document;
  document["device_id"] = deviceId_;
  document["status"] = status;
  document["message"] = message;

  String body;
  serializeJson(document, body);

  const int code = http.POST(body);
  http.end();

  return code >= 200 && code < 300;
}

bool OtaClient::downloadAndInstall(
    const String& downloadUrl,
    size_t expectedSize) {
  WiFiClient client;
  HTTPClient http;

  String fullUrl = downloadUrl.startsWith("http")
      ? downloadUrl
      : serverUrl_ + downloadUrl;

  if (!http.begin(client, fullUrl)) {
    return false;
  }

  const int code = http.GET();

  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const int contentLength = http.getSize();

  if (
      expectedSize > 0 &&
      contentLength > 0 &&
      static_cast<size_t>(contentLength) != expectedSize) {
    http.end();
    return false;
  }

  if (!Update.begin(expectedSize)) {
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  const size_t written = Update.writeStream(*stream);

  if (
      written != expectedSize ||
      !Update.end() ||
      !Update.isFinished()) {
    http.end();
    return false;
  }

  http.end();
  return true;
}

OtaResult OtaClient::checkAndInstall() {
  OtaResult result;

  HTTPClient http;
  const String url =
      serverUrl_ +
      "/api/device/firmware?device_id=" +
      deviceId_;

  if (!http.begin(url)) {
    result.message = "Cannot open OTA check URL";
    return result;
  }

  const int code = http.GET();

  if (code == HTTP_CODE_NO_CONTENT) {
    http.end();
    result.success = true;
    result.message = "No update";
    return result;
  }

  if (code != HTTP_CODE_OK) {
    result.message =
        "OTA check failed: HTTP " + String(code);
    http.end();
    return result;
  }

  JsonDocument document;
  const DeserializationError jsonError =
      deserializeJson(document, http.getString());

  http.end();

  if (jsonError) {
    result.message = "Invalid OTA response";
    return result;
  }

  result.updateAvailable = true;

  const String downloadUrl =
      document["assignment"]["downloadUrl"] | "";
  const size_t expectedSize =
      document["assignment"]["size"] | 0;

  reportStatus("downloading");

  if (!downloadAndInstall(downloadUrl, expectedSize)) {
    reportStatus("failed", "Download or Update failed");
    result.message = "OTA installation failed";
    return result;
  }

  reportStatus("done");
  result.success = true;
  result.message = "OTA complete, restarting";

  delay(500);
  ESP.restart();

  return result;
}
