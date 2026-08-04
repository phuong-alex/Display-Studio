# Display Studio Sprint 1.2.3

This release replaces continuous BLE writes with an application-level stop-and-wait transport.

Each browser-to-device frame contains:

```text
0xA5 | sequence low | sequence high | up to 17 data bytes
```

The ESP32 responds immediately with:

```text
0xA6 | sequence low | sequence high
```

The browser sends the next frame only after receiving the matching ACK. Duplicate frames are acknowledged again but are not appended twice. Each frame is retried up to four times.

Expected serial log:

```text
BLE client connected
BLE RX frame #0: 17 bytes
BLE RX frame #1: 17 bytes
...
BLE RX message complete: ... bytes
BLE command received: set_project
```

The Project schema, NVS format, renderer, and command ACK protocol are unchanged.
