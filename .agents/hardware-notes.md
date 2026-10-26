# Hardware Notes (board của Nghĩa)

## Board đang dùng
- **Tên board:** ESP32-2432S028 (còn gọi "Cheap Yellow Display" / CYD), PCB vàng
- **MCU:** ESP32-WROOM-32 (ESP32 classic, KHÔNG phải S3)
- **Màn hình:** 2.4 inch IPS, cảm ứng điện trở (resistive touch), độ phân giải 240x320
- **Trên board:** khe microSD (TF), nút BOOT + RST, loa (SPEAK), LED RGB, cổng USB-C + micro-USB, các connector P3/CN1/P5 (UART/IO mở rộng)
- **Phụ kiện kèm:** cáp USB, dây JST 2 chân, bút cảm ứng

## Lưu ý khi port sang ESP-Claw
- Repo chưa có board `esp32_2432s028`; board gần nhất để tham khảo: `application/edge_agent/boards/rockbase-iot/nm_cyd_c5` (CYD nhưng dùng ESP32-C5).
- Cần tự viết board def (`board_info.yaml`, `board_peripherals.yaml`, `board_devices.yaml`, `setup_device.c`) cho ESP32 classic.
- Driver thường gặp trên dòng CYD: LCD ILI9341 (SPI), touch XPT2046 (SPI riêng) — cần xác nhận lại pin/driver theo phiên bản board thực tế trước khi code.
- ESP32 classic không có PSRAM mặc định trên module này → RAM hạn chế, cần kiểm tra khả năng chạy edge_agent.
