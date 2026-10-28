# Bản đồ các session / nhánh CYD

Bản chính thức hiện tại: **`application/cyd_ai_chatbot_v5/`** (nhánh `claude/focused-einstein-tba9bs`).
Các nhánh dưới đây là lịch sử; tính năng đáng giữ đã nằm trong v5, trừ khi ghi "chưa gộp".

| Nhánh | Session | Nội dung | Trạng thái |
|---|---|---|---|
| `claude/focused-einstein-tba9bs` | [API key vào](https://claude.ai/code/session_01Lix14q6QLTwJAhFgtYUYG9) + [Khắc phục lại](https://claude.ai/code/session_019ajZNzorKR45vD2DzRquYs) | v4 → **v5**: Bản đồ, Cờ vua, Phá gạch, theme đen | **Đang dùng** |
| `claude/ecstatic-ride-moczry` | [Nâng cấp cyd_ai_chatbot](https://claude.ai/code/session_01Df54cQZNFWomwNiow2bzap) | Launcher icon tròn, Cài đặt, game, mặt AI, CI 3 board (`arduino/cyd_ai_chatbot`) | Đã có trong v4/v5 |
| `claude/sleepy-galileo-j8adv8` | [Thiết kế chatbot AI CYD](https://claude.ai/code/session_01VdAzVTcvpKkg57nx1fmCgL) | Hỗ trợ ESP32-2432S028R, ST7789 mặc định; sketch dò vùng chết Wi-Fi (C5, C3) | Chatbot: đã có trong v5. Wi-Fi dead zone: chưa gộp (sketch riêng) |
| `claude/epic-lovelace-96v739` | [Chương trình tương tự ESP Mosaico](https://claude.ai/code/session_01HK8B1H1LNrrk6X2C9scmPh) | Claude Apps bản đầu (`application/cyd_ai_chatbot`), ghi chú Partition Scheme | Đã có trong v5 |
| `ccr-2ee247ed-8oll3m` | [ESP32 projects integration](https://claude.ai/code/session_01XFTenBEoEpMVu9QYLuXWnf) | CYD AI Safety Hub: CO2 SCD41, cảnh báo sạc xe điện, Telegram (`projects/cyd_ai_safety_hub`) | Chưa gộp — firmware riêng, cần cảm biến ngoài |
| `ccr-7809e02f-9vki96` | [GHI NHO board & MH](https://claude.ai/code/session_01RAwCTxpBdzfd7LwdHqEL5c) | Ghi chú phần cứng ESP32-2432S028 | Nội dung chép vào `HARDWARE.md` |
| `claude/vigilant-lovelace-cdg7mx` | session cũ (01DaJddby76…, 01Sv7HuQDeAm…) | Script `setup_and_flash` (Windows) cho ESP-Claw / ESP-Mosaico | Không liên quan CYD Arduino |
| `main` | — | ESP-Claude (ESP-Claw chạy Claude, OTA) — không chạy được trên CYD (cần PSRAM, flash 8 MB) | — |

Quy ước cho các session sau: sửa CYD trong `application/cyd_ai_chatbot_v5/`, tăng `FW_VERSION` trong
`firmware_version.h`, ghi `CHANGELOG.md`. Không đưa API key / mật khẩu Wi-Fi lên git (`config.h` để trống).
