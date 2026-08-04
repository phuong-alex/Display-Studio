# Beta 10 Reference Firmware

This reference project adds:

- captive portal for Wi-Fi setup
- server URL setup
- device ID and device name setup
- persistent portal values in ESP32 Preferences
- Native Calendar rendering with Vietnamese lunar date payload

## First boot

The ESP32 creates:

```text
SSID: EInkPlatform-Setup
Password: einksetup
```

Connect to the access point and open the captive portal.

Configure:

- Wi-Fi
- Server URL
- Device ID
- Device Name

## Reset configuration

Use the WiFiManager reset function or erase flash before uploading when
a clean first-boot test is required.

## Limitation

This is still reference firmware. Replace the placeholder e-paper pins
and merge the production BIN image routine before using it as the main
production firmware.
