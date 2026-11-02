# So sánh firmware Lily OS "CYD 03" với Claude Apps v5

Nguồn so sánh: `https://heylily.net/esp32/manifest/cyd03_20261020/` (manifest + `bootloader.bin`,
`partition-table.bin`, `xiaozhi.bin`), tải ngày 2026-11-02 rồi phân tích tĩnh: bảng phân vùng, chuỗi văn bản,
tên lớp C++, URL. Không dịch ngược mã máy, không chép code hay tài nguyên của Lily. Các tính năng có ở cột "v5.1"
được viết lại từ đầu, dựa trên chuẩn công khai (SAE J1979 cho OBD-II, thuật toán âm lịch Hồ Ngọc Đức, Open-Meteo...).

## 1. Thông tin firmware CYD 03

| Mục | Lily OS CYD 03 | Claude Apps v5.0.1 |
|---|---|---|
| Tên / phiên bản | "Lily OS - ESP32-2432S028 (CYD)" **2.5.8**, build 20/10/2026 | Claude Apps 5.0.1 (28/10/2026) |
| Nền tảng | Xiaozhi ESP32 (fork), ESP-IDF **v5.5.3**, LVGL | Arduino core 3.3.x, Adafruit GFX |
| Board | `main/boards/esp32-2432s028`, màn **ST7789** 240x320, cảm ứng XPT2046 | ST7789 (mặc định) hoặc ILI9341, XPT2046 |
| Phân vùng | nvs 24 KB, phy_init 4 KB, **factory 4032 KB** (không OTA, không vùng dữ liệu) | Huge APP 3 MB + 1 MB SPIFFS (lưu app) |
| Nạp | 3 file: bootloader 0x1000, partition 0x8000, app 0x10000 | 1 file merged ở 0x0 |
| Âm thanh | `DacAudioCodec` trên GPIO26 (DAC), 6 chế độ "Mẫu A..F" (DAC x1/x2/x4, PDM) | Bíp PWM trên GPIO26 |
| Thẻ SD | Có (`SdSPI`, `/sdcard`) | Không dùng |
| Bluetooth | NimBLE (OBD, TPMS, liên kết điện thoại / dẫn đường) | Không |
| Máy chủ AI | `api.tenclass.net/xiaozhi/ota/`, WebSocket / MQTT, cần **kích hoạt mã thiết bị trên heylily.net** | Claude API trực tiếp (API key của bạn) |

## 2. So sánh từng tính năng

Ký hiệu: ✅ có / đã thêm trong v5.1 · ➖ có cách khác · ❌ không làm được trên CYD hoặc không nên chép (lý do ở cột Ghi chú).

### 2.1 AI / Chat

| Tính năng CYD 03 | v5.0.1 | v5.1 | Ghi chú |
|---|---|---|---|
| Chat AI bằng giọng nói ("Chạm màn hình để nói"), máy chủ Xiaozhi | ➖ chat gõ phím + mặt Claude | ➖ | CYD không có micro; giọng nói của Lily cần máy chủ Xiaozhi + kích hoạt. v5 chat với Claude bằng bàn phím / web / Serial |
| Danh sách vai trò AI (≈50 vai): giáo viên các môn, thông dịch 2 chiều, game "Ai là triệu phú", "Đường lên đỉnh Olympia", bác sĩ, bạn nước ngoài... | ❌ | ✅ **Cài đặt > Vai trò AI**, 51 vai, dùng cho cả Chat và mặt Claude | Viết lại prompt riêng. "Sophia - Bạn gái ảo" đổi thành "bạn tri kỷ" |
| Đổi ngôn ngữ (Việt / English / ไทย) | ❌ | ✅ ngôn ngữ trả lời của Claude (Tự động / Việt / English / Thái phiên âm Latin) | Giao diện vẫn tiếng Việt không dấu: font 5x7 không có chữ Thái |
| Tên bạn, Tự động chào ("Xin chào, bạn của tôi!") | ❌ | ✅ Cài đặt > Cá nhân: tên gửi kèm cho Claude, chào khi bật máy | |
| MCP `self.audio_speaker.set_volume`, `self.get_device_status` | ➖ | ➖ | v5 có "+ App" (Claude viết app) thay cho MCP |
| Profile / emoji / khóa profile | ❌ | ✅ Khóa profile bằng PIN | |

### 2.2 Ứng dụng

| Tính năng CYD 03 | v5.0.1 | v5.1 | Ghi chú |
|---|---|---|---|
| **Radio trực tuyến** (đài VN qua heylily.net, đài Thái HTTP), "Radio tùy chỉnh", hiện LIVE / kbps | ❌ | ✅ **Radio**: MP3 qua HTTP (Shoutcast/Icecast), tên bài (ICY), kbps, thời gian, bộ hiển thị âm thanh, âm lượng, đổi đài | Đài VN của Lily đi qua `heylily.net/music/channel/*.php` (cần mã thiết bị) nên không đưa vào. Thêm đài của bạn ở `http://<IP>/radio`. HTTPS / AAC / HLS không phát được (thiếu RAM, như Lily báo "Đài này cần HTTPS — không đủ bộ nhớ") |
| **Nhạc thẻ SD** ("ĐANG PHÁT · THẺ NHỚ") | ❌ | ✅ **Nhạc SD**: file .mp3 ở thư mục gốc, `/music`, `/nhac` | Cảm ứng chuyển sang SPI mềm để nhường bus VSPI cho thẻ SD |
| UI nhạc / Sound viz | ❌ | ✅ 16 cột theo độ lớn âm | |
| **OBD-II** (ELM327 BLE + Wi-Fi): đồng hồ Vòng tua / Tốc độ / Nước mát / Điện áp / Khí nạp, "Tất cả cảm biến", mã lỗi đã lưu / chờ xác nhận / vĩnh viễn, xóa mã lỗi (chạm 2 lần), số khung (VIN), dữ liệu mẫu, nhớ adapter lần trước, "Màn hình khi nối xong" | ❌ | ✅ **OBD-II** đủ các mục đó: 38 cảm biến J1979 (cùng danh sách với Lily), 21 mô tả mã lỗi tiếng Việt, CAN nhiều khung, ATRV, ATDPN | Adapter Wi-Fi có mật khẩu: v5.1 cho nhập (Lily báo "chưa hỗ trợ") |
| **Áp suất lốp (TPMS)** BLE: 4 lốp + dự phòng, áp cao / thấp, báo động, pin yếu, mất sóng, "x giây trước", đổi vị trí lốp, đơn vị bar / psi / kPa, °C / °F, âm cảnh báo, dữ liệu mẫu | ❌ | ✅ **Lốp xe** đủ các mục đó, ghép cảm biến cho từng lốp | Giải mã loại cảm biến "TPMS" BLE phổ biến (dữ liệu nhà sản xuất 18 byte). Vietmap BT10, Bekubee, Sysgration, Pecham, Wicarlink dùng định dạng riêng chưa công bố: chưa đọc được, gói lạ được ghi ra Serial để bổ sung sau |
| **Dẫn đường** (liên kết app Lily trên điện thoại qua BLE: rẽ trái/phải, vòng xuyến, biển cấm, cây xăng, bệnh viện, công an, camera...) | ➖ Bản đồ OSM + tìm đường | ➖ | Giao thức app điện thoại của Lily là riêng. v5 tự tìm đường trên máy (Nominatim + OSRM) |
| **Đồng hồ** + thời tiết (`weather_clock`, độ ẩm) | ➖ app mẫu "Đồng hồ", "Thời tiết" | ✅ **Đồng hồ**: mặt số / mặt kim, âm lịch, ngày lễ, thời tiết Open-Meteo theo vị trí của bạn | |
| Bấm giờ | ❌ | ✅ trong Đồng hồ (có vòng) | |
| Báo giờ | ❌ | ✅ bíp mỗi giờ 6h-22h | |
| **Lịch âm**: ngày/tháng âm (nhuận), can chi năm/tháng/ngày/giờ, 24 tiết khí, giờ hoàng đạo, lễ dương lịch và âm lịch | ❌ | ✅ **Lịch âm**: lưới tháng + trang chi tiết ngày | Có test trên PC: Tết 2023-2026, tháng nhuận 2023/2025 |
| Trò chơi, Cờ vua, Vẽ, Máy tính, Bộ đàm, Đếm bước, Cân nước, Piano, Ukulele, Lên dây guitar, Gõ nhịp, Ghi âm, Bóng đá, Truyện cổ tích, Bitcoin, Nhà thông minh, Trình chiếu, Camera IP, E-Ink, Giao thông | ➖ v5 có 7 trò chơi + "+ App" | ➖ | Chuỗi này có trong binary nhưng manifest CYD 03 không liệt kê (dùng chung bộ menu với các board S3). Các mục cần micro / cảm biến chuyển động / camera không chạy được trên CYD. Còn lại có thể bảo Claude viết bằng "+ App" (vd. "máy tính bỏ túi", "gõ nhịp 120 bpm") |

### 2.3 Cài đặt & hệ thống

| Tính năng CYD 03 | v5.0.1 | v5.1 | Ghi chú |
|---|---|---|---|
| Độ sáng, xoay màn hình | ✅ | ✅ | |
| Âm lượng (%) | ❌ (bật/tắt) | ✅ thanh âm lượng (bíp + radio + nhạc), "Thử loa" | |
| Giao diện Sáng / Tối | ❌ (chỉ tối) | ✅ | Mặt Claude, trò chơi, bản đồ và app Claude giữ nền tối |
| "Giao diện Menu 1..20" | ❌ | ✅ 20 màu nhấn | |
| Giao diện Chat | ➖ | ➖ | |
| Mã PIN 6 số, Khóa app, Khóa profile | ❌ | ✅ Cài đặt > Bảo mật | Khóa lại sau khi màn hình tắt |
| Tự khởi động (mở app sau khi bật máy) | ❌ | ✅ 9 lựa chọn | OBD tự nối adapter lần trước |
| Vị trí | ➖ cố định Hải Phòng | ✅ gõ địa chỉ, tìm tọa độ trên OSM | |
| Múi giờ tự động (`ip-api.com`) | ❌ (UTC+7) | ✅ tùy chọn | |
| Wi-Fi: nhiều mạng đã lưu (`ssid_manager`) | ❌ (1 mạng) | ✅ 5 mạng, tự chọn mạng mạnh nhất, nhớ BSSID | |
| Chế độ cấu hình Wi-Fi qua điểm phát (192.168.4.1) | ❌ | ✅ điểm phát `CYD-Claude-xxxx` + trang cấu hình + mã QR | |
| Xóa Wi-Fi & cấu hình lại | ❌ | ✅ (chạm 2 lần) | |
| Thông tin: MAC, mã thiết bị, phiên bản | ➖ | ✅ thêm MAC, mã thiết bị | |
| Vuốt trái → phải để thoát | ❌ (nút `<`, BOOT, giữ 1,5 s) | ✅ trên mọi màn không dùng thao tác kéo | |
| Kích hoạt / VIP / "Hết lượt dùng thử" / `api35.php` | — | ❌ | Cơ chế thương mại của Lily, không liên quan firmware của bạn |
| OTA (`api.tenclass.net`) | ❌ | ❌ | Bảng phân vùng Huge APP không có OTA; nạp lại bằng esptool-js |
| Nút Vol- / 4G / SIM / pin, sạc | — | — | Phần cứng của board Lily khác, CYD không có |
| AEC, wake word | — | ❌ | Cần micro |
| Chẩn đoán (lý do khởi động, RAM thấp nhất) | ✅ | ✅ | Đã có từ v5.0 |

## 3. Giới hạn đã biết của v5.1

- Chưa thử trên board thật: phần radio, thẻ SD, Bluetooth chỉ được kiểm tra bằng biên dịch CI. Cần thử
  trên CYD và báo lại lỗi (Serial 115200 in log `[audio]`, `[obd]`, `[tpms]`).
- Radio, OBD và TPMS không chạy cùng lúc (RAM ESP32 không có PSRAM): mở OBD/TPMS sẽ dừng radio.
- OBD qua Wi-Fi tạm rời mạng nhà; thoát màn OBD thì máy tự nối lại.
- Font 5x7 chỉ có ASCII: chữ tiếng Việt hiện không dấu (như v5.0).
