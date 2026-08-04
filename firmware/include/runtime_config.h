#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace RuntimeConfig {
void begin();

uint32_t version();
uint32_t heartbeatIntervalMs();
uint32_t jobIntervalMs();
uint32_t commandIntervalMs();
uint32_t otaIntervalMs();
uint32_t sleepAfterUpdateSeconds();
uint32_t nativeRefreshIntervalMs();

String nativeApp();
String ntpServer();
String timezone();

bool apply(JsonVariantConst configuration);
}
