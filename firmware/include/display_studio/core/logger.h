#pragma once

#include <Arduino.h>

namespace DisplayStudio::Core::Logger {
void begin(uint32_t baud);
void info(const String& message);
void warning(const String& message);
void error(const String& message);
}
