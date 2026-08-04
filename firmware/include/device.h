#pragma once
#include <Arduino.h>
namespace Device {
void begin();
String id();
String macAddress();
String name();
uint32_t freeHeap();
uint32_t freePsram();
}
