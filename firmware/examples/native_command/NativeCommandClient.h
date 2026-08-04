#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

struct NativeCommand {
  bool available = false;
  String id;
  String type;
  JsonDocument payload;
};

class NativeCommandClient {
 public:
  NativeCommandClient(String serverUrl, String deviceId)
      : serverUrl_(std::move(serverUrl)),
        deviceId_(std::move(deviceId)) {}

  NativeCommand poll();
  bool complete(
      const String& commandId,
      bool ok,
      const String& message = "");

 private:
  String serverUrl_;
  String deviceId_;
};
