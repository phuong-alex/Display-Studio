# EInk Platform Firmware Beta 13

Beta 13 fixes the panel reset sequence. It uses the exact GxEPD2 init call that worked in the original calendar firmware:

```cpp
display.init(115200, true, 50, false);
```

It also logs the BUSY pin before and after refresh.

Expected successful log:

```text
E-ink init complete; BUSY=0
Controller RAM written; BUSY before refresh=0
_Update_Full : ...
Refresh returned; BUSY=0
```

`Busy Timeout!` must not appear.
