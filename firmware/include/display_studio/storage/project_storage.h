#pragma once

#include <ArduinoJson.h>

namespace DisplayStudio::Storage {
class ProjectStorage {
public:
    void begin();
    bool save(JsonVariantConst project);
    bool load(JsonDocument& project);
    bool verify(JsonVariantConst expected);
    void clear();
    bool exists();
};

ProjectStorage& projectStorage();
}
