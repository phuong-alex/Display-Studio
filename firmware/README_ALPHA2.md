# Firmware Alpha 2

## BLE commands

```json
{"command":"get_info"}
{"command":"get_config"}
{"command":"set_config","config":{...}}
{"command":"set_time","epochMs":1785850000000,"timezoneOffsetMinutes":420}
{"command":"apply"}
{"command":"reboot"}
{"command":"factory_reset"}
```

## Persistent configuration

Preferences namespace:

```text
displaystudio
```

Key:

```text
scene_config
```

## Runtime

The firmware derives a refresh bucket from:

```text
local minute / refreshMinutes
```

It renders only when the bucket changes.
