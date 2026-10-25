# ESP-Claude: ESP-Claw chạy bằng Claude

Bản gốc tiếng Anh của ESP-Claw: xem `README_ESP-CLAW.md`.

Thiết bị ESP32 "chat ra app", giống ESP-Mosaico: anh mô tả bằng lời (ví dụ "làm đồng hồ đếm ngược
cho buổi họp"), Claude tự viết script Lua, nạp vào máy và chạy luôn. Script đã lưu thì chạy được cả khi mất mạng.

Bản này lấy nền là [espressif/esp-claw](https://github.com/espressif/esp-claw) (Apache-2.0) và chỉnh để
chạy tốt với Claude API.

## Khác gì so với ESP-Claw gốc

| Phần | Thay đổi |
|---|---|
| Model mặc định | `claude-opus-5-5`, thay cho `claude-3-5-haiku-latest` (đã ngừng phục vụ) và `claude-sonnet-4-6` |
| API version | `anthropic-version: 2026-10-21` |
| Prompt caching | Tự đặt `cache_control`: system prompt, danh sách tool và lịch sử chat được tính giá cache ở mỗi bước agent |
| Effort | Mục "reasoning effort" (low / medium / high / xhigh / max) được gửi thành `output_config.effort`; `none` thì không gửi |
| Refusal | Gửi kèm `fallbacks: "default"` (Opus 5.x / Sonnet 5.5 / Fable 5.1). Nếu Claude vẫn từ chối thì báo lỗi rõ ràng, không còn "empty response" |
| Lịch sử chat | Không gửi lại thinking block không có chữ ký (khi đổi từ DeepSeek/Qwen sang Claude giữa chừng) |
| Giao diện web | Claude là lựa chọn đầu tiên và được chọn sẵn |
| CI | `.github/workflows/build-esp-claude.yml`: test backend, build firmware 2 board, đăng lên release `esp-claude-latest` |
| OTA | Component mới `application/edge_agent/components/edge_ota`: cập nhật qua Wi‑Fi, nút trên web, lệnh `ota`, rollback |

Endpoint "Anthropic Compatible" của bên thứ ba (MiniMax...) vẫn chạy như cũ: các trường riêng của Claude chỉ
được gửi khi tên model bắt đầu bằng `claude-`.

File đã sửa: `components/claw_modules/claw_core/src/llm/backends/claw_llm_backend_anthropic.c`,
`components/claw_capabilities/cap_llm_config/src/cap_llm_config.c`, 2 trang `LlmPage.tsx` /
`SetupWizardPage.tsx` (kèm `dist/index.html.gz` build lại), cùng test `claw_core/test_host/test_anthropic_backend.c`.

## Phần cứng nên mua

| Board | Ghi chú |
|---|---|
| **Waveshare ESP32-S3-Touch-AMOLED-2.16** | Màn 2.16" 480×480, giống ESP-Mosaico nhất |
| **ESP32-S3-DevKitC-1 (8MB)** | Rẻ nhất, không có màn hình, chat qua web hoặc Telegram. Không có OTA |

Các board khác có sẵn trong `application/edge_agent/boards/` (M5Stack CoreS3, ESP-BOX-3, LilyGO T-Display S3...).
Muốn build cho board nào thì thêm vào `matrix` trong workflow.

## Nạp firmware (không cần cài gì)

1. Vào Releases, tag `esp-claude-latest`, tải `esp-claude-<tên board>.bin`.
2. Mở Chrome/Edge: https://espressif.github.io/esptool-js/ → **Connect**, chọn cổng COM của board.
3. Flash Address `0x0`, chọn file `.bin`, bấm **Program**. Xong thì bấm nút RESET trên board.

## Cài đặt lần đầu

1. Board phát Wi-Fi tên `esp-claw-xxxx`. Kết nối vào, trang cài đặt sẽ tự mở (nếu không thì vào `http://192.168.4.1`).
2. Nhập Wi-Fi nhà/công ty.
3. Bước LLM: **Anthropic** đã được chọn sẵn. Dán API key lấy tại https://platform.claude.com/settings/keys.
4. Có thể chat ngay trên trang web của board, hoặc nối Telegram ở bước IM.

Đổi model hoặc effort bằng lệnh chat (hoặc trang LLM trên web):

```
/llm setup anthropic <api_key>            # Opus 5.5, mạnh nhất cho việc viết app
/llm model claude-haiku-5-5               # rẻ hơn khoảng 40 lần, hợp việc đơn giản
/llm reasoning low                        # nghĩ ít hơn, trả lời nhanh và rẻ hơn
/llm status
```

Giá tham khảo cho mỗi 1 triệu token (vào / ra): Opus 5.5 là $4 / $20, Haiku 5.5 là $0.10 / $0.50.
Nhờ prompt caching, phần system prompt và tool lặp lại ở mỗi bước chỉ tính giá cache.

## Nhân bản nhiều máy

- Cùng một file `.bin` nạp được cho mọi board cùng loại. Có thể nạp nhiều máy liền tay bằng esptool-js.
- API key và Wi-Fi lưu trong NVS của từng máy, nên máy nào cũng phải làm bước "Cài đặt lần đầu".
  Nên dùng một API key riêng cho mỗi máy (hoặc mỗi khách) để dễ theo dõi chi phí và thu hồi khi cần.
- App Lua đã làm xong trên một máy có thể chép sang máy khác qua trang Files trên web.

## Cập nhật

Có hai cách. Cả hai đều bắt đầu bằng: sửa code, push lên GitHub, CI tự build và thay file trong release `esp-claude-latest`.

**1. Qua USB (mọi board).** Nạp lại file `esp-claude-<board>.bin` ở địa chỉ `0x0` như bước "Nạp firmware".
Cách này ghi đè cả dữ liệu trong máy, nên trước đó hãy tải các app Lua muốn giữ về máy tính qua trang Files.

**2. Qua Wi‑Fi (OTA), chỉ board flash 16MB.** Vào trang web của máy, tab **Status → Firmware → Update Firmware**.
Máy tải `esp-claude-<board>-app.bin`, so với bản đang chạy, nếu khác thì cài rồi tự khởi động lại. Dữ liệu và app Lua giữ nguyên.
Trên cổng serial có lệnh tương đương: `ota --check`, `ota --update`, `ota --update --url https://...`.
- Waveshare AMOLED 2.16 (16MB) có 2 slot app nên OTA được. **ESP32‑S3‑DevKitC‑1 (8MB) chỉ có 1 slot, không OTA được**, nút sẽ bị khóa.
- Nếu bản mới không khởi động xong, bootloader tự quay về bản cũ (rollback).
- OTA chỉ thay phần app. Skill và script Lua dựng sẵn nằm ở phân vùng `system`, muốn đổi chúng thì phải nạp lại qua USB.
- Chỉ nhận địa chỉ `https://`. Nút trên web luôn dùng địa chỉ đã gắn trong firmware, không nhận địa chỉ từ trình duyệt.
- Repo này công khai nên máy tải được file cập nhật từ GitHub Releases mà không cần tài khoản. Tuyệt đối không nhét token hay API key vào firmware hay vào repo.

## Build trên máy tính

```bash
. $IDF_PATH/export.sh            # ESP-IDF v5.5
pip install esp-bmgr-assist
cd application/edge_agent
idf.py bmgr -c ./boards/waveshare -b waveshare_ESP32_S3_Touch_AMOLED_2_16
idf.py build flash monitor
```

Giao diện web: `cd application/edge_agent/components/http_server/frontend_source && pnpm install && pnpm build`.

## Giấy phép

Apache-2.0 (xem `LICENSE`). Mã gốc © Espressif Systems. Nếu bán sản phẩm, giữ nguyên LICENSE và ghi chú
thay đổi ở trên, và không dùng tên/logo "Claude" hay "Espressif" làm thương hiệu sản phẩm.
