# Runtime V2

## Purpose

Runtime V2 serializes all operations that may change device state or block execution. It prevents transport callbacks, timers and user commands from invoking the display driver concurrently.

## Runtime operation

```cpp
enum class RuntimeOperationType {
    RenderActiveScene,
    RefreshActiveScene,
    ActivateScene,
    Reboot,
    FactoryReset
};
```

Each operation contains:

- `type`
- optional `requestId`
- optional `sceneId`
- `source` (`ble`, `scheduler`, `button`, `system`)
- creation timestamp

## State machine

```text
Idle
 -> Queued
 -> Preparing
 -> Rendering
 -> Completing
 -> Idle

Any state
 -> Failed
 -> Idle
```

Only one operation may be in `Preparing`, `Rendering` or `Completing`.

## Queue rules

- BLE framing ACKs never enter Runtime Queue.
- Project installation and checksum verification complete before a render operation is queued.
- Duplicate pending refreshes are coalesced.
- Explicit user render has priority over scheduled refresh.
- Reboot and factory reset wait until the response has been sent.
- Queue overflow must return an explicit error; operations must not be silently dropped.

## Transport gate

Renderer Worker may start only when:

```text
transport receiving == false
transport TX pending == false
renderer busy == false
```

A BLE connection alone does not block rendering. Active message transfer does.

## Completion events

Runtime emits:

- `runtime_operation_started`
- `runtime_operation_completed`
- `runtime_operation_failed`
- `scene_rendered`
- `scene_render_failed`

An operation response must preserve the originating `requestId`.

## Scheduler integration

Scheduler never calls Renderer. It creates a low-priority `RefreshActiveScene` operation. If an equivalent refresh is queued or running, the new request is discarded.

## Phase 2.1 acceptance tests

1. Connect and exchange `get_info` while Runtime is idle.
2. Install Projects containing 1, 2, 3 and 10 Scenes.
3. Explicit apply renders only `activeSceneId`.
4. A second apply while rendering returns busy or remains queued without calling the driver concurrently.
5. BLE frame ACKs continue during queued state.
6. Failed render returns Runtime to Idle.
7. Reconnect after render failure remains possible.
