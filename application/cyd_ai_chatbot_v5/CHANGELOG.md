# Claude Apps cho CYD — lịch sử phiên bản

## v5.1.0 (2026-11-02)

Thêm các tính năng của firmware Lily OS "CYD 03" (2.5.8) cho cùng board ESP32-2432S028. So sánh đầy đủ từng mục:
[`SO_SANH_CYD03.md`](SO_SANH_CYD03.md).

**App mới trên màn hình chính**
- **Đồng hồ**: mặt số / mặt kim, ngày dương + âm lịch, ngày lễ, thời tiết Open-Meteo theo vị trí; **Bấm giờ** có vòng;
  **Báo giờ** (bíp mỗi giờ 6h-22h).
- **Lịch âm**: lưới tháng (ngày âm trong từng ô, lễ, Chủ nhật, hôm nay), trang ngày: can chi năm/tháng/ngày, tiết khí,
  giờ hoàng đạo, tháng nhuận. Thuật toán Hồ Ngọc Đức, có test trên PC.
- **Radio**: MP3 qua HTTP (Shoutcast/Icecast) ra loa qua DAC GPIO26, tên bài, kbps, bộ hiển thị âm thanh, âm lượng.
  Sửa danh sách đài ở `http://<IP>/radio`.
- **Nhạc SD**: phát file .mp3 trên thẻ microSD (thư mục gốc, `/music`, `/nhac`).
- **OBD-II**: adapter ELM327 Bluetooth LE hoặc Wi-Fi; Tổng quan (tốc độ / vòng tua / nước mát / điện áp / khí nạp /
  bướm ga), 38 cảm biến, mã lỗi đã lưu / chờ / vĩnh viễn + xóa, VIN, giao thức, dữ liệu mẫu, nhớ adapter.
- **Lốp xe (TPMS)**: cảm biến BLE, 4 lốp + dự phòng, ghép cảm biến, đổi vị trí, cảnh báo áp cao / thấp / pin yếu /
  mất sóng, âm báo, bar / psi / kPa, °C / °F, dữ liệu mẫu.

**Cài đặt** (giờ có 2 trang)
- Vai trò AI (51 vai: giáo viên, thông dịch, game show...), ngôn ngữ trả lời, tên bạn, vị trí, tự động chào.
- Âm lượng, Thử loa; Giao diện Sáng / Tối, 20 màu nhấn.
- Bảo mật: mã PIN 6 số, Khóa app, Khóa profile.
- Tự khởi động (mở app sau khi bật máy); Đồng hồ: múi giờ tự động theo IP, mặt đồng hồ, đồng bộ giờ.
- Wi-Fi: lưu 5 mạng và tự chọn mạng mạnh nhất, nhớ BSSID, cấu hình qua điểm phát `CYD-Claude-xxxx` (có QR),
  quên từng mạng, Xóa Wi-Fi & cấu hình lại.
- Thiết bị: thêm MAC và mã thiết bị.

**Khác**
- Vuốt trái → phải để quay lại trên mọi màn không dùng thao tác kéo.
- Cảm ứng XPT2046 chạy SPI mềm để bus VSPI dành cho thẻ SD.
- Thư viện mới: `ESP8266Audio`, `NimBLE-Arduino`.
- Lệnh Serial mới: `/radio URL`, `/stop`, `/lich`, `/wifilist`.
- Chưa thử trên board thật: radio, SD, Bluetooth mới qua biên dịch CI.

## v5.0.1 (2026-10-28)

- **Sửa "SSL - A fatal alert message was received from our peer" khi bấm Tìm.** Photon và
  routing.openstreetmap.de chỉ nhận TLS 1.3, còn core arduino-esp32 chỉ có TLS 1.2 nên máy chủ cắt kết nối.
  Tìm địa chỉ chuyển sang Nominatim (nominatim.openstreetmap.org, còn TLS 1.2); tìm đường gọi OSRM qua HTTP
  thường (chỉ gửi 2 tọa độ).
- `net_get`: thêm GET qua HTTP thường; gặp "fatal alert" thì dừng thử lại và báo máy chủ từ chối TLS 1.2.

## v5.0.0 (2026-10-28)

Gộp từ bản v4 (session "API key vào") và bản sửa lỗi Bản đồ.

**Bản đồ / Chỉ đường**
- Hết lỗi thanh nút màn Bản đồ bị đè (nút Doi/Ban do/Tim của màn trước lộ ra ở khe giữa 5 nút).
- Gọi mạng qua `net_get`: kiểm tra Wi-Fi và DNS trước, thử lại 3 lần, đọc lỗi TLS thật thay vì chỉ
  "connection refused"; nếu chỉ hỏng ở bước kiểm chứng chỉ thì thử lại không kiểm (dữ liệu bản đồ công khai).
- Lỗi hiện 2 dòng trên màn Chỉ đường; lý do thiếu nền bản đồ hiện trên dải trạng thái.
- Tải nền bản đồ dừng ngay khi máy chủ không tới được (không chờ hết timeout từng ô).

**Chẩn đoán**
- Số phiên bản ở màn khởi động, Cài đặt > Thiết bị, trang web và Serial.
- Cài đặt > Thiết bị: thêm "khối RAM lớn nhất" (TLS cần ~24 KB liền) và lý do khởi động lại
  (bật nguồn / crash / watchdog / sụt áp...). Lệnh Serial `/status` in thêm RAM thấp nhất từng có.

**Cài đặt**
- CI tự build 2 bản (ST7789, ILI9341) và đăng lên GitHub Releases `cyd-claude-apps-v5-latest`.

## v4 (2026-10-27)
Nền đen chữ trắng, Bản đồ chỉ đường OpenStreetMap (Photon + OSRM, nền tile PNG), Cờ vua, Phá gạch,
chế độ 2 người, app Thời tiết, quét Wi-Fi ổn định hơn. Chi tiết: session "API key vào".
