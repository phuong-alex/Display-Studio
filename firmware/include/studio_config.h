#pragma once

#include <ArduinoJson.h>

namespace StudioConfig {
void begin();
bool save(JsonVariantConst config);
bool load(JsonDocument& config);
void clear();
bool exists();
}
