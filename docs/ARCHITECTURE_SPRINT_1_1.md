# Sprint 1.1 Architecture

## Dependency direction

```text
main
→ Core Application
→ Transport / Runtime
→ Storage / Renderer / Device
```

The renderer does not know about BLE.

The BLE transport does not contain e-ink drawing code.

The storage module does not know about browser or display behavior.

## Compatibility boundary

BLE command names remain unchanged:

```text
get_info
get_config
set_config
set_time
apply
reboot
factory_reset
```

This lets the proven Alpha 2 web validate the refactor before the
Project protocol is changed in Sprint 1.2.
