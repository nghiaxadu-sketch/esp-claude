# Claude Apps cho CYD (Arduino IDE)

"Chat ra app" kiểu ESP-Mosaico, dành cho màn cảm ứng **Cheap Yellow Display 2.8" 320×240**:
bạn mô tả app bằng lời (ví dụ "đồng hồ đếm ngược 25 phút, hết giờ thì nháy đèn đỏ và kêu bíp"),
Claude viết app, máy kiểm tra, tự sửa lỗi nếu cần, lưu vào flash và chạy luôn.
App đã lưu chạy được cả khi mất mạng. Vẫn giữ chế độ chat với Claude như bản cũ.

Sketch tự nhận board theo mục **Tools → Board** (xem `board_pins.h`):

| Board thật | Chọn trong Arduino IDE | Màn hình |
|---|---|---|
| **CYD ESP32-2432S028R bản mới** (có cổng USB-C) | **ESP32 Dev Module** (mặc định) | ST7789 |
| CYD ESP32-2432S028R bản cũ (chỉ có micro-USB) | **ESP32 Dev Module**, đặt `CYD_DISPLAY_ST7789 0` trong `config.h` | ILI9341 |
| RockBase-iot NM-CYD-C5 | **ESP32C5 Dev Module** | ST7789 |

Nâng cấp từ bản "CYD AI Chatbot" cũ: Wi-Fi, API key, model và hiệu chỉnh cảm ứng đã lưu vẫn giữ nguyên.

## Tính năng

- **Màn hình chính (launcher)**: lưới app 3×2, nhiều trang. Chạm để mở; giữ 1 giây để **Chạy / Sửa / Xóa**.
- **+ App**: gõ mô tả trên bàn phím cảm ứng (không dấu cũng được) → Claude viết app → máy chạy thử
  (khởi động, vài nhịp timer, bấm thử từng nút). Nếu lỗi, máy gửi lỗi lại cho Claude sửa (tối đa 2 lần) rồi mới lưu.
- **Sửa app**: mô tả thay đổi ("thêm nút 10 phút", "đổi màu xanh"). Nếu app vừa bị lỗi lúc chạy, lỗi đó được gửi kèm.
- **Thoát app**: giữ màn hình 1,5 giây, hoặc bấm nút **BOOT**.
- **Wi-Fi**: nút **Wi-Fi** quét các mạng xung quanh (vạch sóng, `**` = có mật khẩu), chạm vào mạng → nhập mật khẩu
  → **Noi**. Kết nối được thì lưu lại. Lần đầu chưa có Wi-Fi thì máy tự mở màn này. Mất mạng thì tự nối lại ở nền.
- **Trang web trên máy** `http://<IP của máy>/` (IP hiện trên màn hình chính): dán API key, đổi model,
  **tạo app bằng điện thoại** (gõ tiếng Việt có dấu thoải mái), cài app từ JSON, xem JSON các app.
- **Chat**: như bản cũ (nút **Chat**), nhớ 10 tin gần nhất. Trả lời giờ được *stream* nên không còn bị timeout.
- 4 app mẫu cài sẵn: Đồng hồ, Hẹn giờ, Xúc xắc, Bộ đếm (cũng là ví dụ cho Claude bắt chước).
- Đèn RGB: xanh dương là đang nghĩ, xanh lá là xong, đỏ là lỗi. App có thể tự điều khiển đèn, loa (ESP32-2432S028R
  có cổng loa `SPEAK`), cảm biến ánh sáng, độ sáng màn, đồng hồ NTP và gọi API JSON qua HTTPS (thời tiết...).

## 1. Cài Arduino IDE

1. Arduino IDE 2.x → **File → Preferences → Additional boards manager URLs**, thêm:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. **Boards Manager** → cài **esp32 by Espressif Systems** bản **3.3.x trở lên**.
3. **Library Manager** → cài:
   - `Adafruit ILI9341`
   - `Adafruit ST7735 and ST7789 Library`
   - `Adafruit GFX Library`
   - `ArduinoJson` (bản 7.x)

## 2. Chọn board (Tools)

**CYD ESP32-2432S028R:**

| Mục | Giá trị |
|---|---|
| Board | **ESP32 Dev Module** |
| Partition Scheme | Không cần chọn: file `partitions.csv` trong thư mục sketch tự đặt 3MB cho app + ~900KB lưu app |
| Upload Speed | 921600 (lỗi thì hạ xuống 115200) |

**NM-CYD-C5:**

| Mục | Giá trị |
|---|---|
| Board | **ESP32C5 Dev Module** |
| Flash Size | **16MB (128Mb)** |
| Partition Scheme | Không cần chọn (dùng `partitions.csv` như trên; flash còn dư không dùng đến) |
| PSRAM | **Enabled** |
| USB CDC On Boot | **Enabled** (để Serial Monitor chạy qua cổng USB) |

Sketch cỡ 1.4MB nên partition mặc định (1.25MB) không đủ; `partitions.csv` giải quyết việc này. Nếu thấy báo "Sketch too big" thì kiểm tra file `partitions.csv` còn nằm cạnh `cyd_ai_chatbot.ino` không.

## 3. Nạp và cài đặt

1. Mở `cyd_ai_chatbot.ino`, bấm **Upload**. (Hoặc tải file `.merged.bin` trong mục Artifacts của
   GitHub Actions và nạp bằng https://espressif.github.io/esptool-js/ ở địa chỉ `0x0`.)
2. Lần đầu: màn **Hieu chinh cam ung**, chạm lần lượt 3 dấu `+`. Làm lại: giữ **BOOT** lúc khởi động hoặc gõ `/cal`.
3. Chưa có Wi-Fi thì máy mở màn **Wi-Fi**: chạm vào mạng nhà bạn, gõ mật khẩu, bấm **Noi**.
4. API key (lấy ở https://platform.claude.com/settings/keys): mở `http://<IP>/` trên điện thoại cùng Wi-Fi,
   dán vào ô API key → **Lưu**. Hoặc gõ `/key sk-ant-...` trong Serial Monitor (115200, "Newline").
5. Bấm **+ App**, mô tả app, bấm **Tao**. Khoảng 20–60 giây là app chạy.

## Lệnh Serial

| Lệnh | Tác dụng |
|---|---|
| `/wifi TEN\|MATKHAU` | Lưu Wi-Fi và kết nối lại |
| `/scan` | Quét Wi-Fi xung quanh |
| `/key sk-ant-...` | Lưu API key |
| `/model claude-haiku-5-5` | Đổi model (lưu lại) |
| `/new mo ta app` | Claude viết app mới |
| `/apps`, `/run ID`, `/del ID` | Danh sách / chạy / xóa app |
| `/cal` | Hiệu chỉnh cảm ứng |
| `/clear` | Xóa hội thoại |
| `/status` | Xem Wi-Fi, model, số app, RAM |
| chữ bình thường | Hỏi Claude |

## App được viết thế nào

Mỗi app là một file JSON (`/apps/<id>.json` trên flash): danh sách biến, timer và các phần tử trên màn
(`label`, `button`, `rect`, `circle`, `line`, `bar`). Logic viết bằng một ngôn ngữ script nhỏ kiểu C
(`t -= 1; if (t == 0) { beep(1000, 600); led(255,0,0) }`), không có vòng lặp nên app không bao giờ treo máy.
Toàn bộ quy tắc gửi cho Claude nằm trong `app_builder.cpp` (`kAppFormatGuide`); app mẫu ở `builtin_apps.cpp`.
Biến khai báo trong `"persist"` (điểm cao, bộ đếm...) được lưu lại giữa các lần mở app.

## Model và chi phí

Mặc định `claude-sonnet-5-5`: chat dùng `effort: low`, viết app dùng `effort: medium` (`config.h`).
Có bật `fallbacks: "default"` để Claude tự chạy lại bằng model khác nếu bộ lọc an toàn từ chối nhầm.
Muốn app xịn hơn: `/model claude-opus-5-5`. Rẻ hơn khoảng 20 lần: `/model claude-haiku-5-5`.
Giá mỗi 1 triệu token (vào / ra): Sonnet 5.5 $2 / $10, Opus 5.5 $4 / $20, Haiku 5.5 $0.10 / $0.50.
Một lần tạo app tốn khoảng 5–10 nghìn token (vài trăm đồng với Sonnet).

## Chỉnh trong `config.h`

| Hằng số | Ý nghĩa |
|---|---|
| `CYD_DISPLAY_ST7789` | Chỉ cho ESP32-2432S028R: `1` = ST7789 (bản USB-C, mặc định), `0` = ILI9341 (bản micro-USB) |
| `LCD_ROTATION` | `1` mặc định. Nếu hình bị ngược thì thử `3` |
| `LCD_INVERT_COLORS` | `-1` theo board, `1` nếu màu bị âm bản, `0` nếu không |
| `CHAT_TEXT_SIZE` | `1`: 51 ký tự × 17 dòng. `2`: chữ to |
| `CLAUDE_EFFORT` / `CLAUDE_APP_EFFORT` | Mức suy nghĩ khi chat / khi viết app |
| `APP_REPAIR_ROUNDS` | Số lần Claude được sửa app bị máy từ chối |
| `APP_EXIT_HOLD_MS` | Giữ màn hình bao lâu thì thoát app |
| `APP_LOCATION_TEXT` | Vị trí cho app thời tiết (mặc định Hải Phòng) |
| `WEB_PORTAL_ENABLED` | Bật/tắt trang web trên máy |
| `SPEAKER_ENABLED`, `RGB_LED_ENABLED`, `RGB_LED_LEVEL` | Loa, đèn RGB và độ sáng đèn |

## Gỡ lỗi nhanh

| Hiện tượng | Cách xử lý |
|---|---|
| Màu bị đảo, hình dọc bị cắt, dưới có nhiễu | Sai driver màn: đổi `CYD_DISPLAY_ST7789` (1 ↔ 0) rồi nạp lại |
| Hình quay ngược 180° | Đổi `LCD_ROTATION` thành `3`, rồi gõ `/cal` |
| Chạm lệch | Gõ `/cal` để hiệu chỉnh lại |
| Màn Wi-Fi "Khong thay mang nao" | Bấm **Quet** lại; ESP32 thường chỉ bắt được Wi-Fi 2.4 GHz (C5 bắt được cả 5 GHz) |
| "Khong noi duoc ..." | Sai mật khẩu hoặc sóng yếu; mạng có trang đăng nhập (captive portal) không dùng được |
| "App loi: ..." trên màn chính | Giữ tile 1 giây → **Sua**, gõ "sua loi" là đủ: lỗi được gửi kèm cho Claude |
| `API loi 401` | Sai API key, nhập lại trên trang web hoặc `/key ...` |
| `API loi 429` / `529` | Quá giới hạn hoặc server bận, đợi một lát |
| "no file system partition" trong Serial | Thiếu `partitions.csv` cạnh file .ino |
| "Sketch too big" / "text section exceeds" | Thiếu `partitions.csv` cạnh file .ino (copy cả thư mục `cyd_ai_chatbot`, không chỉ file .ino) |

## Cấu trúc code

| File | Nội dung |
|---|---|
| `cyd_ai_chatbot.ino` | Điểm vào, gọi `chatbotSetup()` / `chatbotLoop()` |
| `chatbot_app.cpp` | Luồng chính: launcher, chat, Wi-Fi, tạo/chạy app, lệnh Serial |
| `launcher_ui.cpp`, `chat_ui.cpp`, `wifi_ui.cpp`, `touch_keyboard.cpp`, `ui_common.cpp` | Các màn hình |
| `mini_json.cpp`, `mini_script.cpp`, `mini_app.cpp` | Bộ chạy app (C++ thuần, có test trên PC) |
| `app_validator.cpp` | Kiểm tra + chạy thử app trước khi lưu |
| `app_builder.cpp` | Prompt cho Claude và vòng tự sửa lỗi |
| `app_device.cpp` | Vẽ lên TFT, hàm thiết bị cho app (LED, loa, giờ, HTTP...) |
| `app_store.cpp`, `builtin_apps.cpp` | Lưu app trên LittleFS/FFat, app mẫu |
| `claude_client.cpp` | Gọi Claude Messages API qua HTTPS (streaming) |
| `wifi_manager.cpp` | Quét và kết nối Wi-Fi |
| `web_portal.cpp` | Trang web cài đặt / tạo app |
| `board_hw.cpp`, `board_pins.h`, `touch_xpt2046.cpp` | Phần cứng từng board |
| `settings_store.cpp`, `text_utils.cpp`, `config.h` | Lưu cài đặt NVS, xử lý chữ, cấu hình |

Test trên PC (không cần board), chạy trong thư mục `test/`:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -I.. ../text_utils.cpp text_utils_test.cpp -o text_utils_test && ./text_utils_test
g++ -std=c++17 -Wall -Wextra -Werror -I.. ../mini_json.cpp ../mini_script.cpp ../mini_app.cpp ../text_utils.cpp \
    ../builtin_apps.cpp ../app_validator.cpp mini_app_test.cpp -o mini_app_test && ./mini_app_test
```

CI: `.github/workflows/build-cyd-chatbot.yml` chạy 2 test trên và compile bằng arduino-cli cho 3 cấu hình
(ESP32-2432S028R ST7789, ESP32-2432S028R ILI9341, NM-CYD-C5). File `.merged.bin` có trong Artifacts.
