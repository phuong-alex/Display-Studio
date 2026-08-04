# EInkPlatform Firmware Beta 10 — Integrated

Đây là firmware đã ghép trực tiếp từ firmware đang chạy thành công của
thiết bị.

## Đã giữ nguyên

- Pin màn hình:
  - CS 10
  - DC 9
  - RST 8
  - BUSY 7
  - SCK 12
  - MOSI 11
- GxEPD2_213_Z98c
- Tải ảnh BIN hai plane
- Hiển thị ảnh đen/đỏ/trắng
- Captive portal Wi-Fi tự viết
- Device Web
- Heartbeat và Image Job hiện tại

## Đã thêm

- Server URL ngay trong captive portal
- Device capabilities
- Runtime configuration từ Dashboard
- Native commands:
  - refresh
  - clear
  - restart
  - sleep
  - native-clock
  - native-calendar
- NTP Native Clock
- Native Calendar có dữ liệu âm lịch từ server
- OTA
- Partition OTA cho flash 16 MB
- Phiên bản `3.0.0-beta.10-integrated`

## Nạp lần đầu

Mở thư mục này bằng VS Code + PlatformIO, sau đó:

```powershell
pio run -e esp32-s3 -t upload
pio device monitor -b 115200
```

Lần nạp đầu bằng USB có thể xóa dữ liệu cũ nếu đổi partition. Nếu upload
báo lỗi partition hoặc thiết bị khởi động lặp, chạy:

```powershell
pio run -e esp32-s3 -t erase
pio run -e esp32-s3 -t upload
```

Sau khi khởi động, thiết bị vẫn dùng Wi-Fi đã lưu. Khi không kết nối được,
nó tạo AP `EInk-Setup-xxxx`; trang cấu hình hiện có cả Wi-Fi và Server URL.

## OTA

OTA không chạy ngay sau khi nạp USB. Thiết bị chờ đủ thời gian OTA interval
mới kiểm tra firmware được gán trên Dashboard, giúp tránh vòng lặp cập nhật
ngay khi thử nghiệm.
