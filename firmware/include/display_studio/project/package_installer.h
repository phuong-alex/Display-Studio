#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <memory>

namespace DisplayStudio::Project {

class PackageInstaller {
public:
    bool install(std::unique_ptr<JsonDocument> project);
    const String& lastError() const;

private:
    String lastError_;
};

PackageInstaller& packageInstaller();

}
