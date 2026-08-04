#include "display_studio/core/logger.h"

namespace DisplayStudio::Core::Logger {
void begin(uint32_t baud) {
    Serial.begin(baud);
    delay(200);
}

void info(const String& message) {
    Serial.println("[INFO] " + message);
}

void warning(const String& message) {
    Serial.println("[WARN] " + message);
}

void error(const String& message) {
    Serial.println("[ERROR] " + message);
}
}
