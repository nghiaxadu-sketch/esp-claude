# Planwork: ESP-Claude Multi-Tool (lấy cảm hứng từ hệ module mở rộng của Flipper Zero)

> Trạng thái: **Draft v1** · Ngày: 2026-10-31 · Chủ trì: Tuấn Nghĩa
> Nhánh làm việc: `ccr-eb15871d-jvxvni`

## 0. Mục tiêu

Biến ESP-Claude (agent Claude chạy trên ESP32-S3) thành một **thiết bị đa năng cầm tay**: một máy chủ có
màn hình + nút, cắm thêm các module rời (IR, Sub-GHz, NFC, GPS, CAN, nRF24, LED badge...). Điểm khác so với
Flipper: **mọi module đều điều khiển được bằng lời** qua Claude ("học remote điều hòa phòng ngủ", "log GPS
mỗi 10 giây", "đọc mã lỗi OBD xe máy"), và Claude tự viết App Lua cho module.

Nguyên tắc kiến trúc (theo `.agents/design.md`):

| Lớp | Vai trò | Vị trí |
|---|---|---|
| Lua module (C) | Driver phần cứng, có Kconfig guard | `components/lua_modules/lua_module_<x>/` |
| Board device | Khai báo chân cắm module | `boards/<vendor>/<board>/board_devices.yaml` |
| Skill | Dạy Claude dùng module (SKILL.md + script mẫu) | `lua_module_<x>/skills/` |
| App Lua | Màn hình thao tác bằng nút, chạy offline | `fatfs_image/system/apps/<x>/` |
| Capability (C) | Chỉ khi agent cần gọi trực tiếp, không qua Lua | `components/claw_capabilities/` |

Không đụng vào `claw_core` trừ khi bắt buộc.

## 1. Phạm vi: những gì LÀM và KHÔNG LÀM

**Làm (phân tích, điều khiển thiết bị của mình, học tập):** quét/đo Wi-Fi & BLE, học/phát IR, thu & giải mã
Sub-GHz (cảm biến thời tiết, chuông cửa của mình), đọc thẻ NFC của mình, GPS logger, đọc CAN/OBD-II ở chế độ
listen-only, logic analyzer/oscilloscope mini, LED badge, game.

**Không làm (out of scope, không nhận PR):**
- Deauth/jamming Wi-Fi, beacon spam, evil portal, bắt handshake mạng người khác (các tính năng "DC33 Deauth",
  "Marauder" phần tấn công).
- Replay/brute-force mã cửa cuốn, khóa xe (rolling code), MouseJack qua nRF24.
- Clone thẻ từ/thẻ ra vào không phải của mình.
- Phát CAN frame lên xe đang chạy.

Lý do: vi phạm Luật Tần số vô tuyến điện, quy định thiết bị vô tuyến cự ly ngắn (Thông tư 08/2021/TT-BTTTT)
và Điều 289 BLHS (xâm nhập trái phép mạng). Thay vào đó làm bản **phòng thủ**: *phát hiện* deauth/beacon
bất thường xung quanh.

## 2. Bản đồ module (theo poster) → hướng làm

| # | Module trên poster | Bản ESP-Claude | Phần cứng gợi ý | Giao tiếp | Có sẵn trong repo? | Phase |
|---|---|---|---|---|---|---|
| 1 | Wi-Fi Devboard / Ghost ESP | Wi-Fi Analyzer (scan AP, kênh, RSSI, độ nghẽn kênh) | Không cần, dùng Wi-Fi của S3 | nội bộ | Một phần (`cmd_wifi`) | P1 |
| 2 | IR Blaster | Học & phát remote, thư viện remote | Module IR LED 940nm + TSOP38238 | RMT | **Có** `lua_module_ir` | P1 |
| 3 | ProtoBoard | Header GPIO/I2C/UART cho DIY | Board đục lỗ 2.54 | GPIO | **Có** gpio/i2c/uart/adc | P1 |
| 4 | LED Badge Board | Bảng chữ chạy / icon | WS2812 8×32 hoặc MAX7219 | RMT / SPI | **Có** `lua_module_led_strip` | P1 |
| 5 | Video Game Module | Game console mini | Chính máy chủ | LVGL | **Có** app `dino`, `flappybird` | P1 |
| 6 | ESP32 Marauder / DC33 (chỉ phần monitor) | Wi-Fi Guard: phát hiện deauth, rogue AP, đếm thiết bị | Không cần | promiscuous RX | Chưa | P2 |
| 7 | AIO/Combo GPS Board | GPS logger, đồng bộ giờ, geofence | u-blox NEO-M8N / M10 | UART NMEA | Có `lua_driver_uart` | P2 |
| 8 | NFC Booster | Đọc NTAG/MIFARE UID, ghi NTAG của mình | PN532 (I2C) | I2C | Chưa | P2 |
| 9 | DigiLab | Logic analyzer 4 kênh + vôn kế / scope mini | Không cần thêm (dùng ADC, PCNT, RMT) | ADC/RMT | Có driver nền | P2 |
| 10 | Feberis Pro / CC1101-HP | Sub-GHz receiver + decoder (433/868 MHz) | CC1101 module (E07-M1101D) | SPI | Chưa | P3 |
| 11 | nRF24 Board / AIO 2.4GHz | Spectrum scanner 2.4 GHz, liên lạc giữa 2 máy | nRF24L01+PA+LNA | SPI | Chưa | P3 |
| 12 | CANBus Shield | OBD-II reader (listen-only mặc định) | SN65HVD230 + TWAI có sẵn của ESP32 | TWAI | Chưa | P3 |
| 13 | ST-Link V3 | Debug: dùng USB-JTAG có sẵn của S3; SWD flasher là tùy chọn | — | USB | Có (USB-JTAG) | P4 (optional) |
| 14 | Ultra Multiboard / AIO Board | Board gộp: CC1101 + nRF24 + GPS + IR trên 1 PCB | PCB tự thiết kế | SPI dùng chung | — | P4 |

## 3. Phần cứng máy chủ (Host)

Đề xuất **LilyGO T-Display S3** (đã có board def `lilygo/lilygo_t_display_s3`) cho bản prototype vì có màn hình,
2 nút, rẻ. Bản "đẹp" về sau: **Waveshare ESP32-S3-Touch-AMOLED-2.16** (16MB, OTA được).

Chuẩn cổng mở rộng (đặt tên **"Claw-Port"**, header 2×8 2.54mm) để mọi module cắm chung:

| Pin | Chức năng | Pin | Chức năng |
|---|---|---|---|
| 1 | 3V3 | 2 | 5V (qua cầu chì) |
| 3 | GND | 4 | GND |
| 5 | SPI SCK | 6 | SPI MOSI |
| 7 | SPI MISO | 8 | SPI CS0 (CC1101) |
| 9 | SPI CS1 (nRF24) | 10 | GPIO IRQ |
| 11 | I2C SDA | 12 | I2C SCL |
| 13 | UART TX | 14 | UART RX |
| 15 | RMT/IR TX | 16 | RMT/IR RX |

Việc cần làm: tạo board def mới `community/esp_claude_multitool` khai báo các device trên trong
`board_peripherals.yaml` / `board_devices.yaml`, để Lua gọi `xxx.new()` không cần nhớ chân.

## 4. Lộ trình theo phase

### P0 — Nền tảng (Tuần 1–2)
- [ ] Board def `esp_claude_multitool` + sơ đồ Claw-Port (mục 3).
- [ ] App **Launcher "Tools"** (Lua/LVGL): menu dạng lưới, mỗi module 1 ô; tự ẩn ô khi module không cắm (dò I2C/SPI ID).
- [ ] Quy ước lưu dữ liệu: `storage.join_path(storage.get_root_dir(), "captures/<module>/...")` (không hard-code `/fatfs`).
- [ ] Thêm board vào `matrix` CI.
- **Nghiệm thu:** build pass, Launcher hiện đủ ô, ô "Không có module" xám đi.

### P1 — Dùng ngay thứ đã có (Tuần 3–5)
- [ ] **IR Remote App**: học remote → đặt tên → lưu JSON; phát lại; thư viện preset (TV Samsung/LG/Sony, điều hòa Daikin/Panasonic).
  Skill: "học remote điều hòa và tạo lịch bật 6h sáng" (kết hợp `cap_scheduler`).
- [ ] **Wi-Fi Analyzer App**: danh sách AP, RSSI, kênh, biểu đồ độ đông kênh 1–13, gợi ý kênh tốt nhất.
- [ ] **BLE Scanner App** (dùng `lua_module_ble`): liệt kê thiết bị, RSSI, nhận diện beacon/AirTag gần mình.
- [ ] **LED Badge App**: gõ chữ trên web/chat → chữ chạy.
- [ ] **ProtoBoard Skill**: hướng dẫn Claude đọc cảm biến I2C bất kỳ (scan địa chỉ, tra datasheet, viết driver Lua).
- **Nghiệm thu:** mỗi App chạy offline bằng nút; mỗi skill có 1 câu lệnh mẫu chạy đúng từ chat.

### P2 — Module dễ, giá trị cao (Tuần 6–9)
- [ ] `lua_module_wifi_guard` (C): promiscuous mode chỉ đếm/khớp frame management → cảnh báo khi phát hiện deauth flood, AP trùng SSID khác BSSID. Đẩy event qua `claw_event_router` → Telegram.
- [ ] `lua_module_gps`: parser NMEA (GGA/RMC), API `gps.fix()`, `gps.track_start(interval)`; xuất GPX vào DATA. Skill: "tóm tắt quãng đường hôm nay".
- [ ] `lua_module_nfc` (PN532): `nfc.read_uid()`, `nfc.read_ntag()`, `nfc.write_ntag(url)`. Use case: tag NFC mở nhanh App/scene.
- [ ] **DigiLab App**: logic analyzer 4 kênh (RMT RX), giải mã UART/I2C cơ bản; vôn kế ADC; xuất CSV.
- **Nghiệm thu:** test Lua trong `test/` cho mỗi module; tài liệu README theo `lua-module-spec.md`.

### P3 — Module RF (Tuần 10–14)
- [ ] `lua_module_cc1101`: cấu hình tần số/điều chế (OOK/2-FSK), chế độ **RX** + RSSI sweep; bộ decoder cho cảm biến nhiệt-ẩm 433 MHz, chuông cửa phổ biến. TX khóa sau Kconfig `CONFIG_CC1101_TX_ENABLE` (mặc định tắt), giới hạn dải ISM.
- [ ] `lua_module_nrf24`: spectrum scanner 126 kênh (RPD), chế độ chat/telemetry giữa 2 máy ESP-Claude.
- [ ] `lua_module_can` (TWAI): mặc định **listen-only**; OBD-II PID (tốc độ, vòng tua, nhiệt độ nước, DTC) chỉ ở chế độ request chuẩn qua cổng OBD; log CAN ra file.
- **Nghiệm thu:** đo thật với cảm biến/xe của mình; không có code path TX ngoài phạm vi mục 1.

### P4 — Phần cứng gộp & hoàn thiện (Tuần 15–20)
- [ ] Thiết kế PCB **Claw Multiboard** (KiCad): CC1101 + nRF24 + GPS + IR + PN532 trên 1 board, đặt JLCPCB.
- [ ] Vỏ in 3D (trong suốt như poster).
- [ ] Tùy chọn: SWD flasher (DAP) cho STM32 — chỉ làm nếu còn thời gian.
- [ ] Trang docs + video demo; release `esp-claude-multitool`.

## 5. BOM dự kiến (giá tham khảo thị trường VN)

| Hạng mục | Linh kiện | Ước tính |
|---|---|---|
| Host | LilyGO T-Display S3 | ~350.000đ |
| IR | LED IR + TSOP38238 + transistor | ~30.000đ |
| GPS | NEO-M8N + anten | ~200.000đ |
| NFC | PN532 V3 | ~120.000đ |
| Sub-GHz | CC1101 433MHz + anten SMA | ~100.000đ |
| 2.4GHz | nRF24L01+PA+LNA | ~60.000đ |
| CAN | SN65HVD230 + đầu OBD-II | ~120.000đ |
| LED badge | WS2812 8×32 | ~250.000đ |
| Phụ kiện | Pin LiPo 1000mAh, dây, header | ~150.000đ |
| **Tổng prototype** | | **~1.4 triệu đồng** |
| PCB + vỏ (P4) | JLCPCB 5 board + in 3D | ~800.000đ |

## 6. Kiểm thử & chất lượng (checklist kiểu QA)

- Build: `idf.py bmgr -b esp_claude_multitool && idf.py build` trên CI cho mỗi PR.
- Mỗi Lua module: test trong `components/lua_modules/<module>/test/` (vd. `gps_nmea_parse.lua`, `ir_roundtrip.lua`).
- Mỗi App: test tay theo bảng `Bước / Kỳ vọng / Kết quả`, chụp màn hình lưu vào PR.
- Rủi ro chính:

| Rủi ro | Ảnh hưởng | Giảm thiểu |
|---|---|---|
| Hết RAM khi chạy LVGL + Wi-Fi + agent | Crash | Dùng PSRAM, tắt module không dùng bằng Kconfig |
| Tranh chấp bus SPI (CC1101 + nRF24 + màn hình) | Mất gói | Dùng SPI host riêng cho RF, mutex trong driver |
| Nhiễu RF từ chính ESP32 | Giảm độ nhạy | Anten ngoài, bố trí PCB xa anten Wi-Fi |
| Kéo dài timeline | Trễ | Phase độc lập, P1–P2 dùng được ngay khi xong |

## 7. Việc ngay tuần này

1. Mua: T-Display S3, module IR, PN532, NEO-M8N (đủ cho P0–P2).
2. Tạo board def `esp_claude_multitool` (copy từ `lilygo_t_display_s3`).
3. Viết Launcher "Tools" + IR Remote App (module IR đã có sẵn, nhanh có kết quả demo nhất).
