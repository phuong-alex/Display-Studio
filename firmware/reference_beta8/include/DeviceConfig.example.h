#pragma once

// Copy this file to DeviceConfig.h and change the values.

#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASSWORD "YOUR_PASSWORD"

#define SERVER_URL "http://192.168.1.20:3000"
#define DEVICE_ID "eink-s3-001"
#define DEVICE_NAME "EInk S3"

// Confirm these pins with your own board.
#define EPD_CS 10
#define EPD_DC 9
#define EPD_RST 8
#define EPD_BUSY 7

#define HEARTBEAT_INTERVAL_MS 30000UL
#define JOB_INTERVAL_MS 5000UL
#define COMMAND_INTERVAL_MS 5000UL
#define OTA_INTERVAL_MS 60000UL
