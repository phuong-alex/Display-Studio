#pragma once

#include <Arduino.h>

namespace DisplayStudio::Config {
inline constexpr uint32_t SERIAL_BAUD = 115200;
inline constexpr uint32_t SPI_FREQUENCY = 4000000;
inline constexpr uint8_t DISPLAY_ROTATION = 1;

inline constexpr uint16_t SCREEN_WIDTH = 250;
inline constexpr uint16_t SCREEN_HEIGHT = 122;

inline constexpr const char* BLE_SERVICE_UUID =
    "7fb90001-6f3e-4e65-9c7a-f13827b6a001";
inline constexpr const char* BLE_RX_UUID =
    "7fb90002-6f3e-4e65-9c7a-f13827b6a001";
inline constexpr const char* BLE_TX_UUID =
    "7fb90003-6f3e-4e65-9c7a-f13827b6a001";

inline constexpr size_t MAX_PROJECT_BYTES = 8192;
}
