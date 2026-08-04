#pragma once
#include <Arduino.h>

namespace Config {
inline constexpr uint32_t SERIAL_BAUD = 115200;
inline constexpr uint32_t SPI_FREQUENCY = 4000000;
inline constexpr uint8_t DISPLAY_ROTATION = 1;

inline constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
inline constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 30000;
inline constexpr uint16_t PORTAL_HTTP_PORT = 80;
inline constexpr uint8_t PORTAL_DNS_PORT = 53;

inline constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;
inline constexpr uint32_t JOB_CHECK_INTERVAL_MS = 10000;
inline constexpr uint32_t HTTP_TIMEOUT_MS = 15000;

inline constexpr const char* PREF_WIFI_NAMESPACE = "eink-wifi";
inline constexpr const char* PREF_KEY_SSID = "ssid";
inline constexpr const char* PREF_KEY_PASSWORD = "password";

inline constexpr const char* PREF_APP_NAMESPACE = "eink-app";
inline constexpr const char* PREF_KEY_SERVER_URL = "server";
inline constexpr const char* PREF_KEY_LAST_JOB = "lastjob";
inline constexpr const char* DEFAULT_SERVER_URL = "http://192.168.1.20:3000";

inline constexpr uint16_t SCREEN_WIDTH = 250;
inline constexpr uint16_t SCREEN_HEIGHT = 122;
inline constexpr size_t PLANE_BYTES = ((SCREEN_WIDTH + 7) / 8) * SCREEN_HEIGHT;
inline constexpr size_t IMAGE_BYTES = PLANE_BYTES * 2;
}
