# Claude Apps cho CYD — lịch sử phiên bản

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
