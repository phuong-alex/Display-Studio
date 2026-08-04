# Streaming Project Upload Protocol V1

## Purpose

Protocol V1 removes the requirement that an entire Project be embedded in one BLE JSON command. The Studio sends a sequence of small commands, while firmware owns one bounded upload buffer.

## Commands

### `upload_begin`

```json
{
  "command": "upload_begin",
  "size": 8192,
  "crc32": 305419896
}
```

The device validates `size`, allocates the upload buffer and resets the received offset to zero.

### `upload_chunk`

```json
{
  "command": "upload_chunk",
  "offset": 0,
  "data": "base64 encoded bytes"
}
```

Chunks must arrive in strict offset order. Version 1 uses 384 raw bytes per Studio chunk. Each command remains comfortably below the previous large-command frame threshold.

### `upload_commit`

The device requires `received == size`, verifies standard CRC32 (`0xEDB88320`), decodes Project JSON once, validates it through `ProjectManager`, saves it as an NVS blob and verifies the stored checksum.

### `upload_abort`

Releases the session buffer without modifying the installed Project.

### `upload_status`

Returns whether a session is active and the current `received` and `expected` byte counts. This is the foundation for resume support; automatic resume is not enabled in the first iteration.

## Compatibility

The legacy `set_project` command remains temporarily available for older Studio builds. New Studio builds use the streaming commands. Project Schema V1 remains unchanged.

## Safety limits

- Maximum Project size: `Config::MAX_PROJECT_BYTES`.
- Only one upload session may exist at a time.
- A disconnect or factory reset aborts the active session.
- CRC failure and invalid JSON never replace the installed Project.
