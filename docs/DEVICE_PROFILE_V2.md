# Device Profile V2

## Purpose

Device Profile describes capabilities without exposing driver implementation details. Studio uses it to validate layouts and firmware uses it to select policies.

## Example

```json
{
  "schema": "display-studio/device-profile-v1",
  "id": "esp32s3-epd-213-z98c",
  "family": "esp32-epd",
  "display": {
    "width": 250,
    "height": 122,
    "colorMode": "bwr",
    "refreshModes": ["full"],
    "rotation": [0, 1, 2, 3]
  },
  "resources": {
    "maxProjectBytes": 16384,
    "maxScenes": 16,
    "maxAssetsBytes": 0
  },
  "transport": ["ble"],
  "capabilities": ["clock", "calendar", "text"]
}
```

## Required sections

- identity: `id`, `family`
- display geometry and color mode
- supported refresh modes
- resource limits
- supported transports
- capabilities

## Studio behavior

Studio must:

- size Preview from the profile;
- reject Projects that exceed hard limits;
- warn when a Widget or refresh mode is unsupported;
- preserve Project content when switching profiles, unless the user explicitly accepts a conversion.

## Runtime behavior

Runtime treats Device Profile as read-only. Driver selection happens during application setup. Project validation may use resource and capability limits but must not inspect concrete driver types.

## Compatibility

Profiles are versioned separately from Projects. Updating a profile cannot change the meaning of an existing Project schema.
