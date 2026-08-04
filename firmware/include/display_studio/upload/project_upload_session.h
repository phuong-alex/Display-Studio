#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace DisplayStudio::Upload {

class ProjectUploadSession {
public:
    bool begin(size_t expectedSize, uint32_t expectedCrc32);
    bool append(size_t offset, const String& base64Data);
    bool commit(JsonDocument& project);
    void abort();

    bool active() const;
    size_t received() const;
    size_t expected() const;

private:
    uint8_t* buffer_ = nullptr;
    size_t expectedSize_ = 0;
    size_t receivedSize_ = 0;
    uint32_t expectedCrc32_ = 0;
    bool active_ = false;

    static uint32_t crc32(const uint8_t* data, size_t length);
};

ProjectUploadSession& projectUploadSession();

}
