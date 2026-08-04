#pragma once
#include <Arduino.h>
namespace WiFiManager {
enum class State { Idle, Connecting, Connected, Portal };
void begin();
void loop();
State state();
bool isConnected();
String ipAddress();
int32_t rssi();
String accessPointName();
}
