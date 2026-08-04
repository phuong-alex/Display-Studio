#include <Arduino.h>
#include <WiFi.h>
#include "device.h"

namespace {
String deviceId;
String deviceName;
}

namespace Device {
void begin() {
    const uint64_t chipId = ESP.getEfuseMac();
    char idBuffer[13];
    snprintf(idBuffer, sizeof(idBuffer), "%04X%08X",
             static_cast<uint16_t>(chipId >> 32),
             static_cast<uint32_t>(chipId));
    deviceId = String(idBuffer);
    deviceName = "EInk-" + deviceId.substring(deviceId.length() - 6);
}

String id() { return deviceId; }
String macAddress() { return WiFi.macAddress(); }
String name() { return deviceName; }
uint32_t freeHeap() { return ESP.getFreeHeap(); }
uint32_t freePsram() { return ESP.getFreePsram(); }
}
