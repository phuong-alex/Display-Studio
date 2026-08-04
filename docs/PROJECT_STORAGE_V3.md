# Project Storage V3

Display Studio 3.0 stores Project Schema V1 payloads as NVS blobs instead of NVS strings.

## Why

Project size grows with each Scene. The earlier `Preferences.putString()` implementation was suitable for small Projects but was not a scalable persistence format for larger multi-Scene payloads. It also required additional `String` and `JsonDocument` copies during installation.

## Contract

- Maximum accepted serialized Project size: 32 KiB.
- Storage key: `project_blob`.
- Stored data: UTF-8 JSON bytes without a trailing null byte.
- The checksum verification step remains mandatory.
- Existing `project_v1` and `scene_config` string keys remain readable for migration.
- A successful blob save removes the older string keys.

## Diagnostics

Firmware logs serialized byte count and free heap during save, load, and install. Hardware validation should test Projects with 1, 3, 5, 10, and 20 Scenes and confirm that byte count, checksum, and free heap remain valid.
