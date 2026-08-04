# Widget API V2

## Goal

Widgets are hardware-neutral renderable units. A Widget may read runtime values and draw to Canvas, but it must not call BLE, storage or a display driver.

## Contract

```cpp
class Widget {
public:
    virtual const char* type() const = 0;
    virtual bool update(const WidgetContext& context) = 0;
    virtual void render(Canvas& canvas, const WidgetFrame& frame) = 0;
    virtual ~Widget() = default;
};
```

## WidgetContext

Provides read-only access to:

- local time
- Project variables
- device capabilities
- cached data-provider values
- locale and timezone

## WidgetFrame

Defines:

- `x`, `y`
- `width`, `height`
- visibility
- z-order
- clipping policy

Coordinates use the logical Canvas coordinate system.

## Registry

Widget types are created through a registry:

```cpp
registry.registerFactory("clock", createClockWidget);
```

Unknown Widget types do not crash rendering. Renderer reports an unsupported-widget warning and continues with remaining Widgets.

## Lifecycle

1. Project is validated.
2. Scene Widgets are instantiated or resolved.
3. `update()` prepares values without drawing.
4. `render()` draws to Canvas.
5. Driver commits the complete Canvas.

## Rules

- Widget rendering is deterministic for the supplied context.
- Widgets cannot trigger a physical refresh.
- Network access belongs to Data Providers, not Widgets.
- Widgets cannot change active Scene directly; they publish an Event or Runtime request.
- Widget-specific configuration is namespaced inside the Widget object.

## Initial built-in Widgets

- clock
- weekday
- date
- text
- image
- QR code
- battery

Clock/calendar preset compatibility will be implemented by converting preset data into these built-in Widgets at runtime.
