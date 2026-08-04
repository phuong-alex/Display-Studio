# Native Command Protocol

Beta 7 adds a queue of device commands.

Poll:

```text
GET /api/device/command?device_id=<DEVICE_ID>
```

Complete:

```text
POST /api/device/command-done
{
  "device_id": "...",
  "command_id": "...",
  "ok": true,
  "message": ""
}
```

Supported command types:

- `refresh`
- `clear`
- `sleep`
- `restart`
- `native-clock`
- `native-calendar`
- `set-brightness`

The ESP32 decides how each command is executed. Unknown commands should
be reported as failed rather than ignored permanently.
