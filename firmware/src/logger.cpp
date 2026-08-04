#include "logger.h"
namespace Logger {
void begin(uint32_t baudRate) { Serial.begin(baudRate); delay(300); Serial.println(); }
void info(const char* m) { Serial.print("[INFO] "); Serial.println(m); }
void info(const String& m) { info(m.c_str()); }
void warning(const char* m) { Serial.print("[WARN] "); Serial.println(m); }
void warning(const String& m) { warning(m.c_str()); }
void error(const char* m) { Serial.print("[ERROR] "); Serial.println(m); }
void error(const String& m) { error(m.c_str()); }
}
