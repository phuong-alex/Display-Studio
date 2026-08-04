# Display Studio Alpha 1 Firmware

Nền firmware: EInk Platform Beta 13 đã ổn định.

Alpha 1 chỉ thêm BLE discovery và Device Info, không thay đổi Image Engine.

## Build / Upload

Mở đúng folder `firmware` bằng VS Code + PlatformIO, sau đó dùng:

```powershell
C:\Users\Phuong Huynh\.platformio\penv\Scripts\platformio.exe run -e esp32-s3 -t upload
```

Serial Monitor:

```powershell
C:\Users\Phuong Huynh\.platformio\penv\Scripts\platformio.exe device monitor -e esp32-s3 -b 115200
```

Log đúng:

```text
[INFO] Display Studio Device
[INFO] 1.0.0-alpha.1
[INFO] BLE advertising: DisplayStudio-XXXX
[INFO] BLE service ready: 7fb90001-...
```

## Alpha 1 BLE commands

```json
{"command":"get_info"}
{"command":"ping"}
```

Scene deploy sẽ được thêm ở Alpha 2 sau khi BLE connection được xác nhận ổn định.
