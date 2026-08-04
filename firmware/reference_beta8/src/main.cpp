#include <Arduino.h>
#include <ArduinoJson.h>
#include <GxEPD2_3C.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>

#include "DeviceConfig.h"

GxEPD2_3C<
  GxEPD2_213_Z98c,
  GxEPD2_213_Z98c::HEIGHT
> display(
  GxEPD2_213_Z98c(
    EPD_CS,
    EPD_DC,
    EPD_RST,
    EPD_BUSY
  )
);

unsigned long lastHeartbeat = 0;
unsigned long lastJobCheck = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastOtaCheck = 0;

String apiUrl(const String& path) {
  return String(SERVER_URL) + path;
}

bool postJson(
    const String& path,
    JsonDocument& document) {
  HTTPClient http;

  if (!http.begin(apiUrl(path))) {
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  String body;
  serializeJson(document, body);

  const int code = http.POST(body);
  http.end();

  return code >= 200 && code < 300;
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println(WiFi.localIP());
}

void sendHeartbeat() {
  JsonDocument document;
  document["device_id"] = DEVICE_ID;
  document["name"] = DEVICE_NAME;
  document["ip"] = WiFi.localIP().toString();
  document["firmware"] = "3.0.0-beta.8-reference";
  document["rssi"] = WiFi.RSSI();

  JsonObject capabilities =
      document["capabilities"].to<JsonObject>();

  capabilities["board"] = "esp32-s3-devkitc-1";
  capabilities["firmware_version"] =
      "3.0.0-beta.8-reference";

  JsonObject displayInfo =
      capabilities["display"].to<JsonObject>();

  displayInfo["driver"] = "GxEPD2_213_Z98c";
  displayInfo["width"] = 250;
  displayInfo["height"] = 122;
  displayInfo["color_mode"] =
      "black-red-white";

  JsonObject features =
      capabilities["features"].to<JsonObject>();

  features["ota"] = true;
  features["native_commands"] = true;
  features["native_clock"] = false;
  features["native_calendar"] = false;

  postJson("/api/device/heartbeat", document);
}

bool reportCommand(
    const String& commandId,
    bool ok,
    const String& message) {
  JsonDocument document;
  document["device_id"] = DEVICE_ID;
  document["command_id"] = commandId;
  document["ok"] = ok;
  document["message"] = message;

  return postJson(
    "/api/device/command-done",
    document
  );
}

void clearDisplay() {
  display.setRotation(0);
  display.setFullWindow();

  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
}

void executeCommand(
    const String& id,
    const String& type,
    JsonVariantConst payload) {
  if (type == "refresh") {
    display.refresh();
    reportCommand(id, true, "Display refreshed");
    return;
  }

  if (type == "clear") {
    clearDisplay();
    reportCommand(id, true, "Display cleared");
    return;
  }

  if (type == "restart") {
    reportCommand(id, true, "Restarting");
    delay(500);
    ESP.restart();
    return;
  }

  if (type == "sleep") {
    const uint32_t minutes =
        payload["minutes"] | 5;

    reportCommand(id, true, "Entering deep sleep");
    delay(300);

    esp_sleep_enable_timer_wakeup(
      uint64_t(minutes) * 60ULL * 1000000ULL
    );

    esp_deep_sleep_start();
    return;
  }

  reportCommand(
    id,
    false,
    "Command is not implemented by this firmware"
  );
}

void pollCommand() {
  HTTPClient http;

  const String url =
      apiUrl(
        "/api/device/command?device_id=" +
        String(DEVICE_ID)
      );

  if (!http.begin(url)) {
    return;
  }

  const int code = http.GET();

  if (code != HTTP_CODE_OK) {
    http.end();
    return;
  }

  JsonDocument document;

  if (
    deserializeJson(document, http.getString())
    != DeserializationError::Ok
  ) {
    http.end();
    return;
  }

  http.end();

  executeCommand(
    document["command"]["id"] | "",
    document["command"]["type"] | "",
    document["command"]["payload"]
  );
}

void pollJob() {
  HTTPClient http;

  const String url =
      apiUrl(
        "/api/device/job?device_id=" +
        String(DEVICE_ID)
      );

  if (!http.begin(url)) {
    return;
  }

  const int code = http.GET();

  if (code != HTTP_CODE_OK) {
    http.end();
    return;
  }

  JsonDocument document;

  if (
    deserializeJson(document, http.getString())
    != DeserializationError::Ok
  ) {
    http.end();
    return;
  }

  http.end();

  if (!(document["update"] | false)) {
    return;
  }

  const int width = document["display"]["width"] | 0;
  const int height = document["display"]["height"] | 0;
  const String colorMode =
      document["display"]["color_mode"] | "";

  if (
    width != 250 ||
    height != 122 ||
    colorMode != "black-red-white"
  ) {
    JsonDocument result;
    result["device_id"] = DEVICE_ID;
    result["job_id"] = document["job_id"] | "";
    result["ok"] = false;
    result["message"] =
        "Display profile is not compatible";

    postJson("/api/device/job-done", result);
    return;
  }

  // Integrate the existing working BIN downloader and display
  // routine here. The Beta 8 server response remains compatible
  // with the original job protocol.
}

void pollOta() {
  HTTPClient http;

  const String checkUrl =
      apiUrl(
        "/api/device/firmware?device_id=" +
        String(DEVICE_ID)
      );

  if (!http.begin(checkUrl)) {
    return;
  }

  const int code = http.GET();

  if (code != HTTP_CODE_OK) {
    http.end();
    return;
  }

  JsonDocument document;

  if (
    deserializeJson(document, http.getString())
    != DeserializationError::Ok
  ) {
    http.end();
    return;
  }

  http.end();

  if (!(document["compatible"] | false)) {
    return;
  }

  const String downloadUrl =
      document["assignment"]["download_url"] | "";
  const size_t expectedSize =
      document["assignment"]["size"] | 0;

  WiFiClient client;
  HTTPClient download;

  if (!download.begin(client, apiUrl(downloadUrl))) {
    return;
  }

  if (download.GET() != HTTP_CODE_OK) {
    download.end();
    return;
  }

  if (!Update.begin(expectedSize)) {
    download.end();
    return;
  }

  const size_t written = Update.writeStream(
    *download.getStreamPtr()
  );

  const bool success =
      written == expectedSize &&
      Update.end() &&
      Update.isFinished();

  download.end();

  JsonDocument result;
  result["device_id"] = DEVICE_ID;
  result["status"] = success ? "done" : "failed";
  result["message"] =
      success ? "OTA completed" : "OTA failed";

  postJson("/api/device/firmware/status", result);

  if (success) {
    delay(500);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  connectWifi();

  display.init(115200);
  clearDisplay();

  sendHeartbeat();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }

  const unsigned long now = millis();

  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = now;
    sendHeartbeat();
  }

  if (now - lastJobCheck >= JOB_INTERVAL_MS) {
    lastJobCheck = now;
    pollJob();
  }

  if (now - lastCommandCheck >= COMMAND_INTERVAL_MS) {
    lastCommandCheck = now;
    pollCommand();
  }

  if (now - lastOtaCheck >= OTA_INTERVAL_MS) {
    lastOtaCheck = now;
    pollOta();
  }

  delay(20);
}
