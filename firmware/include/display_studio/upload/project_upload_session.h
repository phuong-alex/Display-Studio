#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <memory>

namespace DisplayStudio::Upload {

enum class UploadCommitError {
    None,
    IncompleteUpload,
    CrcMismatch,
    JsonParseFailed,
    JsonOverflow,
    AllocationFailed
};

class ProjectUploadSession {
public:
    bool begin(size_t expectedSize, uint32_t expectedCrc32);
    bool append(size_t offset, const String& base64Data);
    std::unique_ptr<JsonDocument> commit();
    void abort();

    bool active() const;
    size_t received() const;
    size_t expected() const;
    UploadCommitError lastCommitError() const;
    const char* lastCommitErrorName() const;

private:
    uint8_t* buffer_ = nullptr;
    size_t expectedSize_ = 0;
    size_t receivedSize_ = 0;
    uint32_t expectedCrc32_ = 0;
    bool active_ = false;
    UploadCommitError lastCommitError_ = UploadCommitError::None;

    static uint32_t crc32(const uint8_t* data, size_t length);
};

ProjectUploadSession& projectUploadSession();

}
