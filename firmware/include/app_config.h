#pragma once
#include <Arduino.h>
namespace AppConfig {
void begin();
String serverUrl();
void setServerUrl(const String& url);
String lastJobId();
void setLastJobId(const String& jobId);
}
