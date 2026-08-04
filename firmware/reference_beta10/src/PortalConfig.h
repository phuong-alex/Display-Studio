#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct PortalValues {
  String serverUrl;
  String deviceId;
  String deviceName;
};

class PortalConfig {
 public:
  bool begin();
  PortalValues load();
  bool save(const PortalValues& values);

 private:
  Preferences preferences_;
};
