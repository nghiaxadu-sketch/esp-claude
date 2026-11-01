# ESP-Claude

ESP32 "chat ra app": mô tả bằng lời (vd. "làm đồng hồ đếm ngược cho buổi họp"), Claude tự viết script Lua,
nạp vào máy và chạy. Script đã lưu vẫn chạy khi mất mạng.

Fork từ [espressif/esp-claw](https://github.com/espressif/esp-claw) (Apache-2.0), tối ưu cho Claude API.
README gốc: `README_ESP-CLAW.md` (EN) / `README_CN.md` (CN).

## Thay đổi so với ESP-Claw

| Phần | Thay đổi |
|---|---|
| Model mặc định | `claude-opus-5-5` (thay `claude-3-5-haiku-latest` đã ngừng, `claude-sonnet-4-6`) |
| API | `anthropic-version: 2026-10-21` |
| Prompt caching | Tự gắn `cache_control` cho system prompt, tool list, lịch sử chat |
| Effort | `reasoning effort` → `output_config.effort` (`none` = không gửi) |
| Refusal | Gửi `fallbacks: "default"`; vẫn từ chối thì báo lỗi rõ, không còn "empty response" |
| Lịch sử chat | Bỏ thinking block không có chữ ký (khi đổi từ DeepSeek/Qwen sang Claude) |
| Web UI | Claude là lựa chọn đầu tiên, chọn sẵn |
| OTA | Component `application/edge_agent/components/edge_ota`: nút web, lệnh `ota`, rollback |
| CI | `.github/workflows/build-esp-claude.yml`: test backend → build 2 board → release `esp-claude-latest` (chỉ từ `main`) |

Endpoint "Anthropic Compatible" bên thứ ba (MiniMax...) không bị ảnh hưởng: trường riêng của Claude chỉ gửi khi model bắt đầu bằng `claude-`.

Code chính: `components/claw_modules/claw_core/src/llm/backends/claw_llm_backend_anthropic.c`,
`components/claw_capabilities/cap_llm_config/src/cap_llm_config.c`, `LlmPage.tsx`, `SetupWizardPage.tsx`,
test `components/claw_modules/claw_core/test_host/test_anthropic_backend.c`.

## Phần cứng

| Board | Flash | OTA | Ghi chú |
|---|---|---|---|
| **Waveshare ESP32-S3-Touch-AMOLED-2.16** | 16MB | Có | Màn 2.16" 480×480, giống ESP-Mosaico nhất |
| **ESP32-S3-DevKitC-1** | 8MB | Không (1 slot app) | Rẻ nhất, không màn, chat qua web/Telegram |

Board khác: `application/edge_agent/boards/` — thêm vào `matrix` trong workflow để CI build.

## Bắt đầu nhanh

1. **Nạp firmware:** tải `esp-claude-<board>.bin` ở release `esp-claude-latest` → mở https://espressif.github.io/esptool-js/
   (Chrome/Edge) → Connect → Flash Address `0x0` → Program → RESET.
2. **Cài đặt:** kết nối Wi-Fi `esp-claw-xxxx` (trang cài đặt tự mở, hoặc `http://192.168.4.1`) → nhập Wi-Fi →
   bước LLM chọn sẵn **Anthropic**, dán API key từ https://platform.claude.com/settings/keys.
3. **Dùng:** chat trên web của board hoặc nối Telegram ở bước IM.

```
/llm setup anthropic <api_key>   # Opus 5.5 – mạnh nhất để viết app
/llm model claude-haiku-5-5      # rẻ ~40 lần, cho việc đơn giản
/llm reasoning low               # nhanh, rẻ hơn
/llm status
```

Giá / 1M token (vào/ra): Opus 5.5 $4/$20 · Haiku 5.5 $0.10/$0.50. Phần lặp lại mỗi bước agent chỉ tính giá cache.

## Nhiều máy

- Một file `.bin` dùng cho mọi board cùng loại; Wi-Fi + API key lưu NVS từng máy → máy nào cũng phải làm bước 2.
- Nên mỗi máy/khách một API key riêng để theo dõi chi phí và thu hồi.
- App Lua chép giữa các máy qua trang **Files** trên web.

## Cập nhật

Sửa code → push `main` → CI build và thay file trong `esp-claude-latest`. Sau đó:

- **USB (mọi board):** nạp lại `esp-claude-<board>.bin` ở `0x0`. **Xoá cả dữ liệu** — tải app Lua về trước qua trang Files.
- **Wi-Fi/OTA (chỉ 16MB):** web → **Status → Firmware → Update Firmware**, hoặc serial `ota --check` / `ota --update [--url https://...]`.
  Giữ nguyên dữ liệu và app Lua; boot lỗi thì tự rollback. Chỉ thay phần app — skill/script dựng sẵn ở phân vùng `system`
  phải nạp USB. Chỉ nhận `https://`; nút web luôn dùng URL gắn trong firmware.

> Repo công khai nên máy tải update từ GitHub Releases không cần đăng nhập. **Không bao giờ** đưa token/API key vào firmware hay repo.

## Build trên máy

```bash
. $IDF_PATH/export.sh            # ESP-IDF v5.5
pip install esp-bmgr-assist
cd application/edge_agent
idf.py bmgr -c ./boards/waveshare -b waveshare_ESP32_S3_Touch_AMOLED_2_16
idf.py build flash monitor
```

Web UI: `cd application/edge_agent/components/http_server/frontend_source && pnpm install && pnpm build`.

## Giấy phép

Apache-2.0 (`LICENSE`), mã gốc © Espressif Systems. Bán sản phẩm: giữ LICENSE + ghi chú thay đổi ở trên,
không dùng tên/logo "Claude" hay "Espressif" làm thương hiệu.
