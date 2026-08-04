#pragma once

#include <ArduinoJson.h>

namespace StudioRuntime {
void begin();
bool configure(JsonVariantConst config);
bool configured();
bool renderNow();
void loop();
void clear();
}
