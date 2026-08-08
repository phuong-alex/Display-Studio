#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace DisplayStudio::Storage {

// ProjectStorage owns persistence only.
//
// It deliberately does not validate Projects, select Scenes, configure the
// Runtime, or render. JSON-aware methods are compatibility adapters around the
// raw blob store so higher layers can be migrated incrementally.
class ProjectStorage {
public:
    void begin();

    // Compatibility JSON adapters.
    bool save(JsonVariantConst project);
    bool load(JsonDocument& project);
    bool verify(JsonVariantConst expected);

    // Raw blob primitives. These are the long-term storage boundary used by
    // the package/repository layers. loadBlob() allocates with malloc(); the
    // caller owns the returned buffer and must free() it.
    bool saveBlob(const uint8_t* data, size_t length);
    bool loadBlob(uint8_t*& data, size_t& length);
    bool verifyBlob(const uint8_t* expected, size_t length);

    // Read-only storage metrics for Runtime Capability / Diagnostics APIs.
    size_t totalBytes();
    size_t usedBytes();
    size_t freeBytes();
    size_t projectBytes();

    void clear();
    bool exists();
};

ProjectStorage& projectStorage();

}
