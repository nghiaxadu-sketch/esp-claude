# CYD AI Chatbot cho NM-CYD-C5 (Arduino IDE)

Chatbot Claude chạy trên board **RockBase-iot NM-CYD-C5** (ESP32-C5, màn 2.8" ST7789 320×240, cảm ứng XPT2046).
Gõ câu hỏi trên bàn phím cảm ứng (hoặc trong Serial Monitor), board gửi lên Claude qua Wi-Fi rồi hiện câu trả lời trên màn.

Sketch này độc lập với firmware ESP-Claw (ESP-IDF) trong repo. Chân GPIO lấy đúng theo
`application/edge_agent/boards/rockbase-iot/nm_cyd_c5/`.

## Tính năng

- Màn chat cuộn được: nút **Len / Xuong**, hoặc chạm nửa trên/dưới vùng chat.
- Bàn phím QWERTY trên màn: `Aa` viết hoa, `123` ra số và ký hiệu, `<-` xóa, **Gui** để gửi.
- Nhớ 10 tin gần nhất để Claude hiểu ngữ cảnh. Nút **Xoa** xóa hội thoại.
- Tiếng Việt: Claude trả lời có dấu, máy tự bỏ dấu khi hiển thị (font có sẵn không có dấu). Gõ không dấu Claude vẫn hiểu.
- Đèn RGB: xanh dương là đang nghĩ, xanh lá là xong, đỏ là lỗi.
- Đồng hồ (NTP, UTC+7) ở góc phải thanh tiêu đề.
- HTTPS xác thực chứng chỉ bằng bộ CA có sẵn trong core ESP32. API key lưu trong NVS của board, không nằm trong code.

## 1. Cài Arduino IDE

1. Arduino IDE 2.x → **File → Preferences → Additional boards manager URLs**, thêm:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. **Boards Manager** → cài **esp32 by Espressif Systems** bản **3.3.x trở lên** (ESP32-C5 chỉ có từ 3.3.0).
3. **Library Manager** → cài:
   - `Adafruit ST7735 and ST7789 Library`
   - `Adafruit GFX Library`
   - `ArduinoJson` (bản 7.x)

## 2. Chọn board (Tools)

| Mục | Giá trị |
|---|---|
| Board | **ESP32C5 Dev Module** |
| Flash Size | **16MB (128Mb)** |
| Partition Scheme | **16M Flash (3MB APP/9.9MB FATFS)** |
| PSRAM | **Enabled** |
| USB CDC On Boot | **Enabled** (để Serial Monitor chạy qua cổng USB) |

Partition mặc định 4MB chỉ có 1.2MB cho app, **không đủ** (Wi-Fi + TLS + màn hình). Nhớ đổi partition.

## 3. Nạp và cài đặt

1. Mở `cyd_ai_chatbot.ino`, bấm **Upload**.
2. Lần đầu máy hiện màn **Hieu chinh cam ung**: chạm lần lượt vào 3 dấu `+` (góc trái trên, phải trên, trái dưới).
   Muốn làm lại thì giữ nút **BOOT** trong 1.5 giây đầu khi khởi động, hoặc gõ `/cal`.
3. Mở **Serial Monitor** (115200 baud, chọn "Newline") và gõ:

   ```
   /wifi TenWifiNhaBan|MatKhauWifi
   /key sk-ant-xxxxxxxx
   ```

   API key lấy ở https://platform.claude.com/settings/keys. Cả hai được lưu trong board, nạp lại sketch không mất.
4. Bấm **Nhap** trên màn, gõ câu hỏi, bấm **Gui**.

Cách khác (không khuyến khích): điền `WIFI_SSID`, `WIFI_PASSWORD`, `ANTHROPIC_API_KEY` trong `config.h`.
**Đừng commit file có API key thật lên GitHub.**

## Lệnh Serial

| Lệnh | Tác dụng |
|---|---|
| `/wifi TEN\|MATKHAU` | Lưu Wi-Fi và kết nối lại |
| `/key sk-ant-...` | Lưu API key |
| `/model claude-haiku-5-5` | Đổi model (lưu lại) |
| `/cal` | Hiệu chỉnh cảm ứng |
| `/clear` | Xóa hội thoại |
| `/status` | Xem Wi-Fi, model, RAM |
| chữ bình thường | Hỏi Claude, câu trả lời hiện cả trên màn và Serial |

## Model và chi phí

Mặc định là `claude-opus-5-5` với `effort: low` (nhanh, đủ cho hỏi đáp ngắn), có bật `fallbacks: "default"`
để Claude tự chạy lại bằng model khác nếu bộ lọc an toàn từ chối nhầm.
Muốn rẻ hơn khoảng 40 lần: `/model claude-haiku-5-5`.
Giá tham khảo mỗi 1 triệu token (vào / ra): Opus 5.5 là $4 / $20, Haiku 5.5 là $0.10 / $0.50.

## Chỉnh trong `config.h`

| Hằng số | Ý nghĩa |
|---|---|
| `LCD_ROTATION` | `1` mặc định (giống ESP-Claw). Nếu hình bị ngược thì thử `3` |
| `LCD_INVERT_COLORS` | Đặt `true` nếu màu bị âm bản (đen thành trắng) |
| `CHAT_TEXT_SIZE` | `1`: 51 ký tự × 17 dòng. `2`: chữ to, 25 ký tự × 9 dòng |
| `LCD_BRIGHTNESS_PERCENT` | Độ sáng đèn nền |
| `CLAUDE_EFFORT` | `low` / `medium` / `high`... càng cao càng kỹ nhưng chậm và tốn hơn |
| `CHAT_HISTORY_MAX_MESSAGES` | Số tin gửi kèm làm ngữ cảnh |
| `RGB_LED_ENABLED`, `RGB_LED_LEVEL` | Bật/tắt và độ sáng đèn RGB |

## Gỡ lỗi nhanh

| Hiện tượng | Cách xử lý |
|---|---|
| Màn trắng / đen | Kiểm tra đã chọn đúng board ESP32-C5. Thử `LCD_INVERT_COLORS true` |
| Chạm lệch | Gõ `/cal` để hiệu chỉnh lại |
| `Loi mang: ...` | Wi-Fi yếu hoặc chặn HTTPS. Gõ `/status` để xem |
| `API loi 401` | Sai API key, gõ lại `/key ...` |
| `API loi 429` / `529` | Quá giới hạn hoặc server bận, đợi một lát rồi hỏi lại |
| Lỗi chứng chỉ TLS | Kiểm tra giờ (NTP). Chỉ để thử, có thể đặt `CLAUDE_TLS_INSECURE 1` |
| Upload báo "Sketch too big" | Chưa đổi Partition Scheme sang 3MB APP |

## Cấu trúc code

| File | Nội dung |
|---|---|
| `cyd_ai_chatbot.ino` | Điểm vào, gọi `chatbotSetup()` / `chatbotLoop()` |
| `chatbot_app.cpp` | Khởi động phần cứng, Wi-Fi, hiệu chỉnh cảm ứng, lệnh Serial, vòng chat |
| `chat_ui.cpp` | Màn chat và bàn phím cảm ứng |
| `claude_client.cpp` | Gọi Claude Messages API qua HTTPS |
| `touch_xpt2046.cpp` | Driver cảm ứng XPT2046 trên SPI dùng chung |
| `text_utils.cpp` | Bỏ dấu UTF-8, bỏ Markdown, ngắt dòng (có test chạy trên PC: `test/`) |
| `settings_store.cpp` | Lưu Wi-Fi, API key, model, hiệu chỉnh vào NVS |
| `board_pins.h` | Chân GPIO của NM-CYD-C5 |
| `config.h` | Cấu hình người dùng |

CI: `.github/workflows/build-cyd-chatbot.yml` chạy test `text_utils` và compile bằng arduino-cli.
File `.bin` đã build có trong mục Artifacts của mỗi lần chạy, nạp được bằng esptool-js ở địa chỉ `0x0`
(dùng file `cyd_ai_chatbot.ino.merged.bin`).
