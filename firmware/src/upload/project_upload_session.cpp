#include <Arduino.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>

#include "display_studio/config.h"
#include "display_studio/core/logger.h"
#include "display_studio/upload/project_upload_session.h"

namespace DisplayStudio::Upload {
namespace {
ProjectUploadSession instance;
}

ProjectUploadSession& projectUploadSession() {
    return instance;
}

uint32_t ProjectUploadSession::crc32(
    const uint8_t* data,
    size_t length
) {
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];

        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint32_t mask =
                -(crc & 1UL);
            crc = (crc >> 1) ^
                (0xEDB88320UL & mask);
        }
    }

    return ~crc;
}

bool ProjectUploadSession::begin(
    size_t expectedSize,
    uint32_t expectedCrc32
) {
    abort();

    if (
        expectedSize == 0 ||
        expectedSize > Config::MAX_PROJECT_BYTES
    ) {
        Core::Logger::error(
            "[UPLOAD] Invalid project size: " +
            String(expectedSize)
        );
        return false;
    }

    buffer_ = static_cast<uint8_t*>(
        malloc(expectedSize + 1)
    );

    if (!buffer_) {
        Core::Logger::error(
            "[UPLOAD] Buffer allocation failed: " +
            String(expectedSize)
        );
        return false;
    }

    expectedSize_ = expectedSize;
    expectedCrc32_ = expectedCrc32;
    receivedSize_ = 0;
    active_ = true;

    Core::Logger::info(
        "[UPLOAD] Session started: bytes=" +
        String(expectedSize_) +
        ", heap=" + String(ESP.getFreeHeap())
    );
    return true;
}

bool ProjectUploadSession::append(
    size_t offset,
    const String& base64Data
) {
    if (!active_ || !buffer_) {
        Core::Logger::error(
            "[UPLOAD] No active session"
        );
        return false;
    }

    if (offset != receivedSize_) {
        Core::Logger::error(
            "[UPLOAD] Offset mismatch: expected=" +
            String(receivedSize_) +
            ", received=" + String(offset)
        );
        return false;
    }

    size_t decodedCapacity =
        (base64Data.length() * 3) / 4 + 4;

    uint8_t* decoded = static_cast<uint8_t*>(
        malloc(decodedCapacity)
    );

    if (!decoded) {
        Core::Logger::error(
            "[UPLOAD] Chunk allocation failed"
        );
        return false;
    }

    size_t decodedLength = 0;
    const int result = mbedtls_base64_decode(
        decoded,
        decodedCapacity,
        &decodedLength,
        reinterpret_cast<const unsigned char*>(
            base64Data.c_str()
        ),
        base64Data.length()
    );

    if (
        result != 0 ||
        receivedSize_ + decodedLength > expectedSize_
    ) {
        free(decoded);
        Core::Logger::error(
            "[UPLOAD] Invalid chunk"
        );
        return false;
    }

    memcpy(
        buffer_ + receivedSize_,
        decoded,
        decodedLength
    );
    free(decoded);

    receivedSize_ += decodedLength;

    Core::Logger::info(
        "[UPLOAD] Chunk accepted: received=" +
        String(receivedSize_) + "/" +
        String(expectedSize_)
    );
    return true;
}

bool ProjectUploadSession::commit(
    JsonDocument& project
) {
    if (
        !active_ ||
        !buffer_ ||
        receivedSize_ != expectedSize_
    ) {
        Core::Logger::error(
            "[UPLOAD] Incomplete upload: " +
            String(receivedSize_) + "/" +
            String(expectedSize_)
        );
        return false;
    }

    const uint32_t actualCrc32 =
        crc32(buffer_, receivedSize_);

    if (actualCrc32 != expectedCrc32_) {
        Core::Logger::error(
            "[UPLOAD] CRC mismatch: expected=" +
            String(expectedCrc32_, HEX) +
            ", actual=" +
            String(actualCrc32, HEX)
        );
        abort();
        return false;
    }

    buffer_[receivedSize_] = 0;

    const DeserializationError error =
        deserializeJson(
            project,
            buffer_,
            receivedSize_
        );

    if (error || project.overflowed()) {
        Core::Logger::error(
            "[UPLOAD] Project JSON invalid: " +
            String(error.c_str())
        );
        abort();
        return false;
    }

    Core::Logger::info(
        "[UPLOAD] CRC verified and JSON decoded"
    );
    abort();
    return true;
}

void ProjectUploadSession::abort() {
    if (buffer_) {
        free(buffer_);
        buffer_ = nullptr;
    }

    expectedSize_ = 0;
    receivedSize_ = 0;
    expectedCrc32_ = 0;
    active_ = false;
}

bool ProjectUploadSession::active() const {
    return active_;
}

size_t ProjectUploadSession::received() const {
    return receivedSize_;
}

size_t ProjectUploadSession::expected() const {
    return expectedSize_;
}

}
