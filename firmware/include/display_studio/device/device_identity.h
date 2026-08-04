#pragma once

#include <Arduino.h>

namespace DisplayStudio::Device {
class DeviceIdentity {
public:
    void begin();
    const String& id() const;

private:
    String id_;
};

DeviceIdentity& deviceIdentity();
}
