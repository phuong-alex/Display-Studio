# Event Bus V2

## Purpose

Event Bus distributes notifications about completed or observed state changes. It does not execute blocking work and does not replace Runtime Queue.

## Event envelope

```cpp
struct Event {
    EventType type;
    EventSource source;
    uint32_t timestampMs;
    String correlationId;
};
```

Payloads are typed per event and should remain small.

## Initial events

- transport connected/disconnected
- Project installed/cleared
- active Scene changed
- time synchronized
- runtime operation started/completed/failed
- display driver ready/failed
- scheduled refresh due

## Rules

- Publishers do not assume subscribers exist.
- Subscribers must return quickly and may enqueue Runtime operations.
- Event handlers cannot call Display Driver methods.
- Events are facts in past tense; commands are imperatives and belong in Runtime Queue.
- Repeated high-frequency events may be coalesced.

## Example

```text
Time synchronized (event)
 -> Scheduler recalculates next due time
 -> Scheduler later enqueues RefreshActiveScene (operation)
 -> Renderer completes
 -> Scene rendered (event)
```
