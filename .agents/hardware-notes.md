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

## Giới hạn phần cứng (đánh giá 2026-10)
- CPU: dual-core Xtensa LX6, 240 MHz. Không có lệnh vector/AI như S3/P4.
- RAM: 520 KB SRAM, không có PSRAM. Heap trống sau khi bật WiFi + TLS chỉ còn khoảng 100-150 KB.
- Flash: thường là 4 MB.
- Radio: WiFi 2.4 GHz, BT Classic, BLE 4.2.
- Không có mic, camera, USB-OTG. USB chỉ là cầu nạp/serial (CH340).
- Loa: DAC nội GPIO26 qua amp. Có cảm biến ánh sáng LDR (GPIO34) và LED RGB (4/16/17, active-low).
- Lưu ý: PCB ghi 2432S028 (bản 2.8") nhưng listing ghi 2.4". Cần đo lại màn thật. Bản 2.4" là ESP32-2432S024.

### Pinout phổ biến của CYD 2432S028R (cần xác nhận bằng đồng hồ)
- LCD (HSPI): SCK14 MOSI13 MISO12 CS15 DC2 BL21
- Touch XPT2046 (bus riêng): CLK25 MOSI32 MISO39 CS33 IRQ36
- SD (VSPI): SCK18 MOSI23 MISO19 CS5
- Chân trống cho mở rộng: GPIO22, 27, 35 (35 chỉ input), qua connector P3/CN1

## Kết luận với ESP-Claw
- Chưa chạy được full edge_agent: framework cần PSRAM (SPIRAM_* bật mặc định trong sdkconfig.defaults) và flash >= 8 MB (riêng partition app ota_0 đã là 3800K).
- Hướng tận dụng tối đa: dùng board làm thin client (LVGL UI + touch + gọi LLM API qua HTTPS/WebSocket), hoặc làm terminal/satellite cho một node ESP-Claw khác (S3/P4/C5) hay cho PC.
