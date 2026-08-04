#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace DisplayStudio::Project {
class ProjectChecksum {
public:
    static uint32_t calculate(
        JsonVariantConst project
    );

    static String hex(
        uint32_t value
    );
};
}
