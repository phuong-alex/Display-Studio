#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
#include <memory>

#include "display_studio/config.h"
#include "display_studio/core/logger.h"
#include "display_studio/upload/project_upload_session.h"

namespace DisplayStudio::Upload {
namespace {
ProjectUploadSession instance;

String heapSnapshot() {
    return "free=" + String(ESP.getFreeHeap()) +
        ", largest=" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) +
        ", min=" + String(ESP.getMinFreeHeap());
}
}

ProjectUploadSession& projectUploadSession() { return instance; }

uint32_t ProjectUploadSession::crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint32_t mask = -(crc & 1UL);
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

bool ProjectUploadSession::begin(size_t expectedSize, uint32_t expectedCrc32) {
    abort();
    lastCommitError_ = UploadCommitError::None;
    if (expectedSize == 0 || expectedSize > Config::MAX_PROJECT_BYTES) return false;

    buffer_ = static_cast<uint8_t*>(malloc(expectedSize + 1));
    if (!buffer_) {
        Core::Logger::error("[UPLOAD] Buffer allocation failed: " + heapSnapshot());
        return false;
    }

    expectedSize_ = expectedSize;
    expectedCrc32_ = expectedCrc32;
    receivedSize_ = 0;
    active_ = true;
    Core::Logger::info("[UPLOAD] Session started: bytes=" + String(expectedSize_) + ", " + heapSnapshot());
    return true;
}

bool ProjectUploadSession::append(size_t offset, const String& base64Data) {
    if (!active_ || !buffer_ || offset != receivedSize_) return false;

    const size_t decodedCapacity = (base64Data.length() * 3) / 4 + 4;
    uint8_t* decoded = static_cast<uint8_t*>(malloc(decodedCapacity));
    if (!decoded) return false;

    size_t decodedLength = 0;
    const int result = mbedtls_base64_decode(
        decoded, decodedCapacity, &decodedLength,
        reinterpret_cast<const unsigned char*>(base64Data.c_str()),
        base64Data.length()
    );

    if (result != 0 || receivedSize_ + decodedLength > expectedSize_) {
        free(decoded);
        return false;
    }

    memcpy(buffer_ + receivedSize_, decoded, decodedLength);
    free(decoded);
    receivedSize_ += decodedLength;
    Core::Logger::info("[UPLOAD] Chunk accepted: received=" + String(receivedSize_) + "/" + String(expectedSize_));
    return true;
}

std::unique_ptr<JsonDocument> ProjectUploadSession::commit() {
    lastCommitError_ = UploadCommitError::None;
    Core::Logger::info("[COMMIT] begin: " + heapSnapshot());

    if (!active_ || !buffer_ || receivedSize_ != expectedSize_) {
        lastCommitError_ = UploadCommitError::IncompleteUpload;
        return nullptr;
    }

    const uint32_t actualCrc32 = crc32(buffer_, receivedSize_);
    if (actualCrc32 != expectedCrc32_) {
        lastCommitError_ = UploadCommitError::CrcMismatch;
        abort();
        return nullptr;
    }

    auto project = std::make_unique<JsonDocument>();
    if (!project) {
        lastCommitError_ = UploadCommitError::AllocationFailed;
        abort();
        return nullptr;
    }

    buffer_[receivedSize_] = 0;
    const DeserializationError error = deserializeJson(
        *project,
        reinterpret_cast<const char*>(buffer_),
        receivedSize_
    );

    if (error) {
        lastCommitError_ = UploadCommitError::JsonParseFailed;
        Core::Logger::error("[COMMIT] JSON parse failed: " + String(error.c_str()));
        abort();
        return nullptr;
    }
    if (project->overflowed()) {
        lastCommitError_ = UploadCommitError::JsonOverflow;
        abort();
        return nullptr;
    }

    Core::Logger::info(
        "[COMMIT] parsed once: scenes=" +
        String((*project)["scenes"].as<JsonArrayConst>().size()) +
        ", " + heapSnapshot()
    );

    abort();
    return project;
}

void ProjectUploadSession::abort() {
    if (buffer_) free(buffer_);
    buffer_ = nullptr;
    expectedSize_ = 0;
    receivedSize_ = 0;
    expectedCrc32_ = 0;
    active_ = false;
}

bool ProjectUploadSession::active() const { return active_; }
size_t ProjectUploadSession::received() const { return receivedSize_; }
size_t ProjectUploadSession::expected() const { return expectedSize_; }
UploadCommitError ProjectUploadSession::lastCommitError() const { return lastCommitError_; }

const char* ProjectUploadSession::lastCommitErrorName() const {
    switch (lastCommitError_) {
        case UploadCommitError::None: return "none";
        case UploadCommitError::IncompleteUpload: return "incomplete_upload";
        case UploadCommitError::CrcMismatch: return "crc_mismatch";
        case UploadCommitError::JsonParseFailed: return "json_parse_failed";
        case UploadCommitError::JsonOverflow: return "json_overflow";
        case UploadCommitError::AllocationFailed: return "allocation_failed";
        default: return "unknown";
    }
}

}
