# EInk Platform Firmware Beta 12

Beta 12 fixes image-job display updates for `GxEPD2_213_Z98c`.

## Main change

The downloaded image is 250 x 122 in landscape coordinates. The panel
controller is 128 x 250. Beta 12 rotates both black and red planes into
the controller's native layout, writes them with the driver's native
`writeImage()` API, and performs an explicit full refresh.

## Upload

```powershell
pio run -e esp32-s3 -t upload
pio device monitor -b 115200
```

A successful image job now logs lines similar to:

```text
Direct EPD write: native=128x250, bytes=4000, black_px=..., red_px=..., checksum=...
Controller RAM written; starting full refresh
Full refresh command completed
```

The server remains Beta 11.4; no server replacement is required.
