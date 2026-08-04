# Profile-aware image jobs

The job response now includes display metadata:

```json
{
  "update": true,
  "job_id": "...",
  "image_url": "...",
  "display": {
    "profile_id": "epd-213-z98c",
    "width": 250,
    "height": 122,
    "color_mode": "black-red-white",
    "driver": "GxEPD2_213_Z98c",
    "orientation": 0
  }
}
```

Before drawing, firmware should verify that width, height and color mode
match the compiled display driver. Reject incompatible jobs safely.
