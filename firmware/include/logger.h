#pragma once
#include <Arduino.h>
namespace Logger {
void begin(uint32_t baudRate);
void info(const char* message);
void info(const String& message);
void warning(const char* message);
void warning(const String& message);
void error(const char* message);
void error(const String& message);
}
