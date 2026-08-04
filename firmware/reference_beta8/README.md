# Beta 8 Reference Firmware

This is a reference PlatformIO project for:

- ESP32-S3
- GxEPD2
- GxEPD2_213_Z98c
- heartbeat capability reporting
- native command execution
- OTA polling
- profile compatibility checks

## Before compiling

1. Copy:

```text
include/DeviceConfig.example.h
```

to:

```text
include/DeviceConfig.h
```

2. Enter Wi-Fi and server settings.
3. Confirm all e-paper pins.
4. Confirm the partition scheme supports OTA.
5. Merge the working BIN download/display routine from your current
   firmware into `pollJob()`.

The actual pins were intentionally not assumed because ESP32-S3 e-paper
boards use different wiring.
