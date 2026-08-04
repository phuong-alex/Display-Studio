# Display Studio V2 Roadmap

## Completed

### 2.0 Foundation Phase 1

- Transport-first startup.
- Explicit `apply` rendering only.
- Project schema V1 compatibility.
- Hardware validation with one, two and three Scenes.

## Active

### 2.1 Architecture Pack and Runtime Queue

- Freeze V2 layer contracts.
- Add Runtime operation queue.
- Route `apply` through Renderer Worker.
- Add busy, completion and failure reporting.
- Prove transport remains responsive around render operations.

## Planned

### 2.2 Autonomous Scheduler

- Restore independent clock/calendar refresh.
- Scheduler enqueues refresh only when due.
- Coalesce duplicate refreshes.
- Persist schedule state safely.

### 2.3 Widget Engine

- Widget registry and context.
- Clock, date, weekday and text Widgets.
- Preset compatibility adapter.

### 2.4 Canvas and Layout Engine

- Hardware-neutral Canvas.
- Position, size, clipping, alignment and z-order.
- Studio preview parity tests.

### 2.5 Assets

- Images and fonts.
- Asset manifest, transfer and storage limits.

### 2.6 Connectivity

- Wi-Fi setup and optional NTP.
- BLE remains the recovery/configuration transport.

### 2.7 OTA and Device Management

- Signed firmware update flow.
- Device health and rollback policy.

### 2.8 Additional Drivers

- OLED/TFT proof of architecture.
- LED matrix proof of architecture.

## V3 direction

- MQTT and Home Assistant integrations.
- TrueNAS-hosted fleet management.
- Plugin distribution and community packages.

## Release rule

A milestone cannot be marked complete until its acceptance tests pass on supported hardware and its public contracts are documented.
