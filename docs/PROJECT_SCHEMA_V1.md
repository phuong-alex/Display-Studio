# Project Schema V1

## Compatibility promise

`display-studio/project-v1` is frozen for Display Studio 2.x. New optional fields may be added, but existing required fields cannot be removed or reinterpreted.

## Root

```json
{
  "schema": "display-studio/project-v1",
  "projectVersion": 1,
  "name": "My Display Project",
  "activeSceneId": "main",
  "metadata": {},
  "scenes": []
}
```

Required fields:

- `schema`
- `projectVersion`
- `name`
- `activeSceneId`
- non-empty `scenes`

## Scene

```json
{
  "id": "main",
  "name": "Main Scene",
  "enabled": true,
  "config": {
    "schema": "display-studio/device-config-v1",
    "scene": {}
  }
}
```

Rules:

- Scene IDs are unique and stable.
- `activeSceneId` must reference an enabled Scene.
- Disabled Scenes remain stored but cannot be activated.
- Scene order is presentation order and must not define identity.

## Current scene configuration

The current firmware reads:

- `scene.preset`
- `scene.appearance.accent`
- `scene.content.lunarText`
- `scene.runtime.timezone`
- `scene.runtime.refreshMinutes`
- optional `scene.widgets`

Preset rendering remains a compatibility adapter. Widget Engine will consume `widgets` directly later.

## Migration

Firmware must:

1. Detect legacy `display-studio/device-config-v1` documents.
2. Wrap them in a Project containing one `main` Scene.
3. Store the migrated Project only after validation succeeds.

## Size and validation

- Project size must remain within the device profile limit.
- Unknown optional fields are preserved.
- Unknown required schema versions are rejected.
- Storage verification compares canonical Project checksums.

## Future schema

Breaking changes require `display-studio/project-v2` and an explicit migration path. V1 files must never silently acquire V2 meaning.
