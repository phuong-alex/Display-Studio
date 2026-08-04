# OTA Client Integration Example

Beta 6 adds the server-side OTA protocol. The existing firmware does not
update automatically until the OTA client is integrated.

Required ESP32 flow:

1. Poll:

```text
GET /api/device/firmware?device_id=<DEVICE_ID>
```

2. When HTTP 200 is returned, read:

- version
- board
- size
- sha256
- downloadUrl

3. Report:

```text
POST /api/device/firmware/status
{
  "device_id": "...",
  "status": "downloading"
}
```

4. Download the BIN and write it using Arduino `Update`.

5. Verify the SHA-256 value before reboot.

6. Report `done` or `failed`.

Use an ESP32 partition table with OTA slots. The default single-app
partition table cannot perform OTA.
