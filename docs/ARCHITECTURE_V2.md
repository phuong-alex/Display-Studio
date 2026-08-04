# Display Studio V2 Architecture

## Vision

Display Studio is a local-first platform for designing and deploying display Projects to embedded and Linux devices. A deployed device must continue operating without the Studio or a server.

## Layer model

```text
Studio
  -> Project Document
  -> Transport
  -> Command Dispatcher
  -> Project Store
  -> Runtime Queue
  -> Renderer Worker
  -> Canvas
  -> Display Driver
  -> Hardware
```

## Dependency rule

Dependencies point downward only. A lower layer must never call into a higher layer.

- Studio knows Project schema and device capabilities.
- Transport moves messages but does not validate Projects or render.
- Project Store persists Project documents but does not schedule work.
- Runtime decides what operation should run and when.
- Renderer converts an active Scene to Canvas operations.
- Driver converts Canvas operations to hardware output.

## Core invariants

1. Only Renderer Worker may call a Display Driver refresh method.
2. BLE callbacks may validate framing and enqueue work only.
3. No blocking display operation may run while transport is receiving a command.
4. A Project may contain many Scenes, but exactly one active Scene is rendered per render request.
5. The device owns the deployed Project and can run independently.
6. Hardware-specific APIs are forbidden in Project, Runtime and Widget contracts.
7. Existing Project schema V1 remains readable throughout Display Studio 2.x.

## Modules

### Transport

Owns BLE, future Wi-Fi and other command channels. It emits complete command envelopes into the dispatcher and sends operation responses.

### Command Dispatcher

Validates command names and arguments, performs short non-blocking operations and enqueues runtime work. It never invokes a display refresh directly.

### Project Store

Validates, stores, reloads and verifies Projects. It exposes immutable views of the installed Project and active Scene.

### Runtime Queue

Serializes render, scene activation, refresh, reboot and reset operations. It provides deduplication and clear busy states.

### Renderer Worker

Consumes one runtime operation at a time. It prepares data, renders the active Scene and reports completion.

### Canvas

Provides hardware-neutral drawing primitives. Widgets target Canvas, not GxEPD2 or any other driver.

### Display Driver

Implements the Canvas backend and physical refresh lifecycle for one display family.

## V2 implementation phases

- Foundation 1: transport-first boot and explicit apply only. Hardware-tested with three Scenes.
- Architecture Pack: freeze contracts and runtime behavior.
- Runtime Queue: move explicit apply through a serialized worker.
- Scheduler: enqueue autonomous refresh only when transport and renderer are idle.
- Widget Engine: replace preset-specific rendering with registered Widgets.
- Driver Expansion: add TFT, OLED and LED matrix backends without changing Core.

## Change policy

Any change that crosses layer boundaries requires an Architecture Decision Record in `docs/adr/`. Bug fixes that preserve contracts do not require an ADR.
