#pragma once

#include <Arduino.h>

namespace StudioTime {
void begin();
bool setFromBrowser(
    int64_t epochMs,
    int timezoneOffsetMinutes
);
bool ready();
bool getLocalTime(tm& value);
int timezoneOffsetMinutes();
}
