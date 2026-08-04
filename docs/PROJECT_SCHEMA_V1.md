# Project Schema V1

## Root

| Field | Purpose |
|---|---|
| `schema` | Must be `display-studio/project-v1` |
| `projectVersion` | Project document revision |
| `name` | Human-readable Project name |
| `activeSceneId` | Scene executed by the device |
| `metadata` | Studio and migration metadata |
| `scenes` | One to sixteen Scene records |

## Scene

```json
{
  "id": "main",
  "name": "Main Scene",
  "enabled": true,
  "config": {}
}
```

In Sprint 1.2, `config` retains the proven Alpha 2 scene format. Sprint
1.3 will make Scene a first-class model. Sprint 1.4 will replace preset
rendering with a Widget collection.
