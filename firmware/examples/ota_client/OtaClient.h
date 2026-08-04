#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClient.h>

struct OtaResult {
  bool updateAvailable = false;
  bool success = false;
  String message;
};

class OtaClient {
 public:
  OtaClient(String serverUrl, String deviceId)
      : serverUrl_(std::move(serverUrl)),
        deviceId_(std::move(deviceId)) {}

  OtaResult checkAndInstall();

 private:
  bool reportStatus(
      const String& status,
      const String& message = "");
  bool downloadAndInstall(
      const String& downloadUrl,
      size_t expectedSize);

  String serverUrl_;
  String deviceId_;
};
