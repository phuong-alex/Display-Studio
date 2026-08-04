#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace NativeApp {
void begin();
void loop();

bool showClock(JsonVariantConst payload);
bool showCalendar(JsonVariantConst payload);
}
