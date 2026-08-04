# Display Studio V1 Architecture Freeze

## Purpose

Display Studio is a local-first display platform. The web application designs and deploys Projects. The ESP32 stores the deployed Project and runs it independently after Bluetooth disconnects.

This document freezes the V1 module boundaries so later work can add Scenes, Widgets and additional display types without repeatedly restructuring the codebase.

## Current system map

```text
Web Studio
├── Project form and preview
├── BLE request client
└── Deploy workflow
        │
        ▼
BLE Transport
├── GATT connection
├── frame transfer
├── command request IDs
└── device notifications
        │
        ▼
Core Application
├── startup order
└── main loop
        │
        ├── Device Identity
        ├── Project Manager
        ├── Project Storage
        ├── Time Service
        ├── Scene Runtime
        └── Display Renderer
```

## Firmware modules

### `Core::Application`

Responsibilities:

- Define deterministic startup order.
- Start device identity, storage, time, runtime, project manager, renderer and transport.
- Run transport and runtime loops.

It must not contain BLE framing, Project validation, storage serialization or display drawing rules.

### `Device::DeviceIdentity`

Responsibilities:

- Produce the stable device ID.
- Provide the suffix used in the BLE advertising name.

It must not know about Projects, Scenes or rendering.

### `Transport::BleTransport`

Responsibilities:

- Own GATT service and characteristics.
- Receive and transmit transport frames.
- Reassemble complete command messages.
- Dispatch complete commands.
- Emit command responses and transport status.

It must not implement Project validation, NVS persistence or display drawing.

V1 rule: transport framing and application command responses are separate concepts. Transport ACKs acknowledge bytes only. Command responses acknowledge completed operations.

### `Project::ProjectModel`

Responsibilities:

- Define `display-studio/project-v1`.
- Validate Project structure.
- Resolve and change `activeSceneId`.
- Migrate legacy config documents.

It must not read NVS or render the display.

### `Project::ProjectManager`

Responsibilities:

- Install and load Projects.
- Coordinate validation, Scene selection, persistence verification and runtime configuration.
- Expose the installed Project and active Scene.

It is the orchestration boundary for Project lifecycle operations.

### `Storage::ProjectStorage`

Responsibilities:

- Serialize and store Project JSON in NVS.
- Load stored Projects.
- Verify persisted content.
- Remove old and current Project keys during reset.

It must not choose the active Scene or render it.

### `Runtime::TimeService`

Responsibilities:

- Maintain browser-synchronized time.
- Apply timezone offset.
- Expose local time readiness.

Future NTP or RTC sources must be added behind this service rather than directly inside widgets or renderers.

### `Runtime::SceneRuntime`

Responsibilities:

- Hold the active Scene configuration.
- Determine when a refresh is due.
- Prepare runtime values such as time and date.
- Request rendering through the renderer interface.

Sprint 1.3 will make Scene a first-class model. Sprint 1.4 will replace preset-specific rendering with Widget execution.

### `Renderer::DisplayRenderer`

Responsibilities:

- Own the current GxEPD2 device adapter.
- Draw boot, waiting and active Scene output.
- Report render completion.

Future e-ink, LED matrix, TFT and OLED adapters must implement the same renderer boundary. Project and transport code must not depend on GxEPD2.

## Web modules

### BLE client

Responsibilities:

- Connect to `DisplayStudio-*` devices.
- Encode requests and match responses by `requestId`.
- Transfer frames reliably.
- Cancel pending operations on disconnect.

### Project builder

Responsibilities:

- Create valid Project documents.
- Preserve Project metadata and active Scene selection.

### Deploy workflow

Responsibilities:

- Install Project.
- Synchronize time.
- Apply/render the active Scene.
- Report success only after device responses confirm all requested operations.

## Dependency direction

Allowed dependency direction:

```text
Core Application
  → Transport
  → Project Manager
  → Runtime
  → Storage
  → Renderer
  → Device services
```

Cross-cutting utility modules such as Logger and configuration constants may be used by all modules.

Forbidden dependencies:

- Renderer → BLE transport
- Storage → Renderer
- Device identity → Project manager
- Web preview → firmware implementation details
- Widget code → GATT APIs

## Stable V1 contracts

The following contracts are frozen for V1 unless an explicit Architecture Decision Record changes them:

- Project root schema: `display-studio/project-v1`
- Device command envelope: newline-delimited JSON with `command` and optional `requestId`
- Device response envelope: JSON message with `type`, operation status and matching `requestId` where applicable
- Persistent ownership: ESP32 stores the deployed Project
- Server independence: normal runtime must not require the Studio or TrueNAS server
- Renderer abstraction: display technology is replaceable behind the renderer boundary

## Known transport issue at Sprint 1.2.3

The current stop-and-wait implementation multiplexes binary frame ACKs and JSON notifications on the same TX characteristic. Firmware also executes command handling from the BLE write callback after the final frame is assembled. The observed repeated timeout around a late frame indicates this design still needs stabilization before it can be declared frozen.

The next implementation step must isolate reception from command execution:

```text
BLE callback
→ validate and enqueue frame/message
→ return immediately

main loop
→ dequeue complete command
→ execute Project operation
→ enqueue response

TX pump
→ send one notification at a time
```

This change belongs inside `BleTransport` and a command queue; it must not alter Project, Storage, Runtime or Renderer contracts.

## Planned evolution

### Sprint 1.3

- First-class Scene model.
- Multiple Scene management in the Studio.
- Active Scene switching without changing Project schema.

### Sprint 1.4

- Widget interface and registry.
- Clock and Calendar converted from presets into widgets.

### Later

- Scheduler.
- Image/font assets.
- Wi-Fi/NTP and RTC time providers.
- Additional renderer adapters.
- TrueNAS hosting, fleet and OTA as optional services.
