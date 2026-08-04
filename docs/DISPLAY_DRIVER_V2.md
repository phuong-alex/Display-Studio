# Display Driver V2

## Responsibility

A Display Driver owns hardware initialization, pixel transfer and physical refresh. It contains no Project, Scene, Widget, BLE or scheduler logic.

## Contract

```cpp
class DisplayDriver {
public:
    virtual bool begin(const DeviceProfile& profile) = 0;
    virtual Canvas& canvas() = 0;
    virtual bool commit(const RefreshPolicy& policy) = 0;
    virtual bool busy() const = 0;
    virtual void sleep() = 0;
    virtual ~DisplayDriver() = default;
};
```

## Canvas primitives

The minimum Canvas API includes:

- clear
- drawPixel
- drawLine
- drawRectangle
- fillRectangle
- drawText
- drawBitmap
- clipping

Canvas color values are logical colors. The driver maps unsupported colors according to Device Profile policy.

## Refresh policy

Refresh policy may request:

- full refresh
- partial refresh
- automatic selection
- no physical refresh for buffered displays

A driver may reject unsupported refresh modes explicitly.

## E-ink rules

- Physical refresh is called only by Renderer Worker.
- BUSY waits are contained inside the driver.
- Driver failure returns control to Runtime and must not start an automatic retry loop.
- Recovery is explicit and driver-specific.
- Transport remains operational while Runtime reports driver failure.

## Future implementations

- `GxEPD2Driver`
- `LovyanGfxDriver`
- `RgbMatrixDriver`
- `Ssd1306Driver`
- `LinuxFramebufferDriver`

Core code must not include headers from these implementations.
