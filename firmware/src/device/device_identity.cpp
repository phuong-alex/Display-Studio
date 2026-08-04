#include <Arduino.h>
#include <WiFi.h>

#include "display_studio/device/device_identity.h"

namespace DisplayStudio::Device {
namespace {
DeviceIdentity instance;
}

DeviceIdentity& deviceIdentity() {
    return instance;
}

void DeviceIdentity::begin() {
    const uint64_t chipId =
        ESP.getEfuseMac();

    char buffer[13];

    snprintf(
        buffer,
        sizeof(buffer),
        "%04X%08X",
        static_cast<uint16_t>(
            chipId >> 32
        ),
        static_cast<uint32_t>(chipId)
    );

    id_ = String(buffer);
    id_.toUpperCase();
}

const String& DeviceIdentity::id() const {
    return id_;
}
}
