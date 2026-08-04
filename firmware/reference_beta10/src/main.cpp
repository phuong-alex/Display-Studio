#include <Arduino.h>
#include <ArduinoJson.h>
#include <GxEPD2_3C.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "DeviceDefaults.h"
#include "LunarDate.h"
#include "PortalConfig.h"

using DisplayType = GxEPD2_3C<
  GxEPD2_213_Z98c,
  GxEPD2_213_Z98c::HEIGHT
>;

DisplayType display(
  GxEPD2_213_Z98c(
    EPD_CS,
    EPD_DC,
    EPD_RST,
    EPD_BUSY
  )
);

PortalConfig portalConfig;
PortalValues portalValues;

String apiUrl(const String& path) {
  return portalValues.serverUrl + path;
}

void startConfigurationPortal() {
  WiFiManager manager;

  char serverBuffer[128];
  char deviceIdBuffer[64];
  char deviceNameBuffer[64];

  portalValues.serverUrl.toCharArray(
    serverBuffer,
    sizeof(serverBuffer)
  );

  portalValues.deviceId.toCharArray(
    deviceIdBuffer,
    sizeof(deviceIdBuffer)
  );

  portalValues.deviceName.toCharArray(
    deviceNameBuffer,
    sizeof(deviceNameBuffer)
  );

  WiFiManagerParameter serverParameter(
    "server",
    "Server URL",
    serverBuffer,
    sizeof(serverBuffer)
  );

  WiFiManagerParameter deviceIdParameter(
    "device_id",
    "Device ID",
    deviceIdBuffer,
    sizeof(deviceIdBuffer)
  );

  WiFiManagerParameter deviceNameParameter(
    "device_name",
    "Device Name",
    deviceNameBuffer,
    sizeof(deviceNameBuffer)
  );

  manager.addParameter(&serverParameter);
  manager.addParameter(&deviceIdParameter);
  manager.addParameter(&deviceNameParameter);

  const bool connected = manager.autoConnect(
    CONFIG_AP_NAME,
    CONFIG_AP_PASSWORD
  );

  if (!connected) {
    delay(1000);
    ESP.restart();
  }

  portalValues.serverUrl =
      serverParameter.getValue();

  portalValues.deviceId =
      deviceIdParameter.getValue();

  portalValues.deviceName =
      deviceNameParameter.getValue();

  portalConfig.save(portalValues);
}

void renderNativeCalendarFromPayload(
    JsonVariantConst payload) {
  const String day =
      payload["local"]["day"] | "--";
  const String month =
      payload["local"]["month"] | "--";
  const String year =
      payload["local"]["year"] | "----";
  const String weekday =
      payload["local"]["weekday"] | "";

  const int lunarDay =
      payload["lunar"]["day"] | 0;
  const int lunarMonth =
      payload["lunar"]["month"] | 0;
  const bool lunarLeap =
      payload["lunar"]["leap"] | false;

  display.setRotation(0);
  display.setFullWindow();

  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);

    display.setTextColor(GxEPD_RED);
    display.setTextSize(4);
    display.setCursor(10, 42);
    display.print(day);

    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(2);
    display.setCursor(92, 28);
    display.print("/");
    display.print(month);

    display.setCursor(92, 52);
    display.print(year);

    display.setTextSize(1);
    display.setCursor(10, 76);
    display.print(weekday);

    display.setCursor(10, 100);
    display.print("Lunar: ");
    display.print(lunarDay);
    display.print("/");
    display.print(lunarMonth);

    if (lunarLeap) {
      display.print(" leap");
    }
  } while (display.nextPage());
}

void sendHeartbeat() {
  HTTPClient http;

  if (!http.begin(
      apiUrl("/api/device/heartbeat")
  )) {
    return;
  }

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  JsonDocument request;
  request["device_id"] = portalValues.deviceId;
  request["name"] = portalValues.deviceName;
  request["ip"] = WiFi.localIP().toString();
  request["firmware"] =
      "3.0.0-beta.10-reference";
  request["rssi"] = WiFi.RSSI();

  JsonObject capabilities =
      request["capabilities"].to<JsonObject>();

  capabilities["board"] =
      "esp32-s3-devkitc-1";
  capabilities["firmware_version"] =
      "3.0.0-beta.10-reference";

  JsonObject features =
      capabilities["features"].to<JsonObject>();

  features["ota"] = true;
  features["native_commands"] = true;
  features["native_clock"] = true;
  features["native_calendar"] = true;

  String body;
  serializeJson(request, body);

  http.POST(body);
  http.end();
}

void pollCommand() {
  HTTPClient http;

  const String url =
      apiUrl(
        "/api/device/command?device_id=" +
        portalValues.deviceId
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
    deserializeJson(
      document,
      http.getString()
    ) != DeserializationError::Ok
  ) {
    http.end();
    return;
  }

  const String type =
      document["command"]["type"] | "";

  if (type == "native-calendar") {
    renderNativeCalendarFromPayload(
      document["command"]["payload"]
    );
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  portalConfig.begin();
  portalValues = portalConfig.load();

  startConfigurationPortal();

  display.init(115200);
  sendHeartbeat();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    return;
  }

  static unsigned long lastHeartbeat = 0;
  static unsigned long lastCommand = 0;
  const unsigned long now = millis();

  if (now - lastHeartbeat >= 30000UL) {
    lastHeartbeat = now;
    sendHeartbeat();
  }

  if (now - lastCommand >= 5000UL) {
    lastCommand = now;
    pollCommand();
  }

  delay(20);
}
