# Sprint 1.3 — Queued BLE Transport

## Goal

Move all slow command execution and JSON notification transmission out of the BLE write callback.

## Runtime flow

```text
BLE onWrite callback
├── validate frame
├── append payload
├── queue transport ACK
├── queue complete command
└── return immediately

BleTransport::loop
├── transmit queued ACK
├── execute one queued command
└── pump one JSON notification chunk
```

## Constraints

- Project schema remains `display-studio/project-v1`.
- Web frame format remains `0xA5 | sequence | payload`.
- Transport ACK remains `0xA6 | sequence`.
- Request IDs and command response JSON remain unchanged.
- Project, Storage, Runtime and Renderer APIs remain unchanged.
