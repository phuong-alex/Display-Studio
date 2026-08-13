# PR-015 Diagnostics Panel

Goal: expose Runtime Capability API metrics in Display Studio so normal health checks do not require Serial Monitor.

Scope:
- Diagnostics panel in Web Studio
- Manual Refresh button
- Auto refresh while connected
- Copy Diagnostics text
- Health summary derived from heap/storage values
- Cleanup of remaining LittleFS temp-file warning

No BLE protocol version bump; command `get_runtime_info` was introduced by PR-014.
