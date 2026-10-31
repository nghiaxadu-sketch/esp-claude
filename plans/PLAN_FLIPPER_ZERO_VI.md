# Planwork: Flipper Zero × ESP-Claude

> Trạng thái: **Draft v1** · Ngày: 2026-10-31 · Chủ trì: Tuấn Nghĩa
> Nhánh: `ccr-eb15871d-jvxvni` · Bổ sung cho [`PLAN_MULTITOOL_VI.md`](PLAN_MULTITOOL_VI.md) (không thay thế)

## 0. Mục tiêu

Flipper Zero của mình là máy chủ "nghịch phần cứng" có sẵn; ESP-Claude là bộ não Claude + Wi-Fi. Kế hoạch này làm 3 việc,
độc lập nhau, mỗi việc dùng được riêng:

| Track | Nội dung | Cần gì | Rủi ro |
|---|---|---|---|
| **A. Asset pack "NeonDancer"** | Bộ giao diện riêng cho Flipper: animation, icon, font, passport. Chủ đề nhảy/nhạc + game retro + cyberpunk | Flipper + thẻ SD + PC | Thấp |
| **B. Màn hình phụ / bạn đồng hành** | Board ESP-Claude cắm vào GPIO Flipper, hiển thị trạng thái và nhận lệnh bằng lời | Flipper + 1 board ESP32-S3 (sau đó S31) | Trung bình (phải viết app FAP phía Flipper) |
| **C. Cầu nối dữ liệu** | Nhập file `.ir` của Flipper vào thư viện remote của ESP-Claude | Chỉ cần PC + file `.ir` | Thấp |

Ngoài phạm vi (cùng giới hạn mục 1 của plan Multi-Tool, không nhận PR): phát lại Sub-GHz/rolling code của người khác,
clone thẻ không phải của mình, BadUSB tấn công, phần tấn công của Marauder (deauth, beacon spam, evil portal).

## 1. Track A — Asset pack NeonDancer

**Phase A0 — Xác nhận máy (làm trước, 15 phút)**
- [ ] Mở qFlipper (hoặc màn hình Flipper) ghi lại: firmware đang chạy (Official / Momentum / khác) + số phiên bản. *Hiện chưa biết, đây là điều kiện chặn của cả track.*
- [ ] Có thẻ SD chưa; dung lượng; đã format bằng Flipper chưa.
- [ ] Nếu là firmware Official: asset pack **không** dùng được, cần cài Momentum (momentum-fw.dev) bằng qFlipper/web updater trước. Sao lưu SD trước khi đổi firmware.
- **Nghiệm thu:** điền 3 ô trên vào mục 6.

**Phase A1 — Dựng pipeline vẽ bằng code (1 buổi)**
- [ ] Thư mục `flipper/asset_pack_neondancer/` + script Python (Pillow) sinh ảnh 1-bit: màn Flipper là **128×64, đen/trắng**, nên mọi tác phẩm phải đọc được ở 1-bit.
- [ ] Kiểm tra bố cục thư mục theo README asset pack của Momentum (`Anims/`, `Icons/`, `Fonts/`, `Passport/`) và định dạng frame/manifest *ngay trên bản Momentum đang dùng*, không nhớ từ trí nhớ.
- [ ] Preview trên PC: xuất GIF phóng 4× để duyệt trước khi chép vào SD.
- **Nghiệm thu:** 1 animation 6–8 frame hiển thị đúng trên máy thật.

**Phase A2 — Nội dung (2–3 buổi)**
- [ ] 3 animation chủ đề: dancer neon (nhảy), pixel arcade, cyberpunk skyline.
- [ ] Bộ icon menu + 1 font pixel (kiểm tra giấy phép font trước khi dùng).
- [ ] Passport (ảnh hồ sơ) cho 3 trạng thái cảm xúc.
- **Nghiệm thu:** chọn pack trong cài đặt Momentum, xoay đủ 3 animation, không lỗi hiển thị/không treo.

**Phase A3 — Đóng gói (0,5 buổi)**
- [ ] README + ảnh chụp; zip thả thẳng vào thẻ SD; ghi chú cách gỡ (xóa thư mục pack, quay về pack mặc định).
- **Rủi ro:** pack lỗi có thể làm menu hiển thị xấu hoặc reboot → luôn giữ pack mặc định + bản sao lưu SD.

## 2. Track B — ESP-Claude gắn vào GPIO Flipper

### 2.1 Đấu nối (đối chiếu lại với sơ đồ chân chính thức tại docs.flipper.net trước khi cắm)

| Flipper (header 18 chân) | ESP | Ghi chú |
|---|---|---|
| GND (8, 11 hoặc 18) | GND | Bắt buộc nối chung mass |
| 13 (TX) | RX của ESP | Mức logic 3,3 V |
| 14 (RX) | TX của ESP | Chéo TX↔RX |
| 9 (3V3) hoặc 1 (5V) | cấp nguồn board ESP | Xem dòng tiêu thụ ở dưới, ưu tiên nguồn ngoài |

- Mức logic ESP32 là 3,3 V: **không** đưa 5 V vào chân GPIO ESP.
- Wi-Fi + màn hình có thể kéo dòng đỉnh vài trăm mA, lớn hơn mức nên rút từ Flipper → cấp nguồn riêng (pin/USB) cho board, chỉ nối mass + 2 dây UART.

### 2.2 Phase B0 — Chứng minh đường truyền (1 buổi)
- [ ] Board ESP32-S3 (dùng board đã có trong repo, build được trên CI: `esp32_S3_DevKitC_1`) in dòng chữ qua UART.
- [ ] Phía Flipper dùng app UART có sẵn để thấy chữ; chưa viết code Flipper.
- **Nghiệm thu:** gõ ở Flipper → thấy ở Serial Monitor PC và ngược lại, 115200 baud, 100 dòng không mất ký tự.

### 2.3 Phase B1 — Giao thức + Lua (2 buổi)
- [ ] Giao thức dòng JSON (1 dòng = 1 gói, giới hạn 256 byte): `{"t":"status","wifi":-62,"batt":87}`, `{"t":"cmd","name":"ir_send","arg":"ac_on"}`. Whitelist lệnh phía ESP, không chạy lệnh tùy ý nhận từ cổng.
- [ ] Module Lua `flipper_link` (dùng `lua_driver_uart` có sẵn): `link.send(tbl)`, `link.on_message(fn)`; test trong `components/lua_modules/<module>/test/`.
- **Nghiệm thu:** từ chat ra lệnh "gửi trạng thái Wi-Fi sang Flipper", Flipper nhận đúng JSON.

### 2.4 Phase B2 — App FAP trên Flipper (3–5 buổi, khó nhất)
- [ ] Dựng môi trường `ufbt` trên PC; app C hiển thị 4 dòng trạng thái từ ESP + 2 nút gửi lệnh.
- [ ] Phải khớp **đúng phiên bản firmware** đang chạy (A0), FAP biên dịch cho bản này có thể không chạy trên bản khác.
- **Nghiệm thu:** bấm nút trên Flipper → ESP-Claude phát IR; ngắt dây → Flipper hiện "Mất kết nối", không treo.

### 2.5 Phase B3 — Chuyển sang ESP32-S31 (sau cùng, tùy chọn)
- Board ESP32-S31-Function-CoreBoard-1 là phần cứng mới: repo hiện chỉ có board def `esp32_s31_korvo1` (board âm thanh khác), **chưa có** def cho CoreBoard-1. Việc cần làm: tạo board def (copy từ `esp32_s31_korvo1`), đối chiếu chân với tài liệu board, rồi mới đưa vào CI.
- Chỉ chuyển khi B0–B2 chạy ổn trên S3, để không dồn rủi ro phần cứng mới lên rủi ro giao thức.

## 3. Track C — Cầu nối file `.ir`

- [ ] Script PC `tools/flipper_ir_import.py`: đọc file `.ir` của Flipper (văn bản, mỗi nút một khối `name/type/protocol/address/command` hoặc dạng raw), xuất JSON thư viện remote của ESP-Claude.
- [ ] Hỗ trợ giao thức phổ biến (NEC, Samsung32, RC5, SIRC) và raw; giao thức lạ thì bỏ qua + báo rõ ở cuối.
- [ ] Test vòng tròn: nhập → phát bằng `lua_module_ir` → thu lại → so khớp.
- **Nghiệm thu:** 1 remote TV thật của mình, ≥ 80% nút chạy; nút lỗi được liệt kê, không im lặng bỏ qua.
- **Lưu ý:** chỉ nhập remote thiết bị của mình/được phép; không nhập, không lưu `.sub` (Sub-GHz).

## 4. Lộ trình gợi ý

| Tuần | Việc | Ghi chú |
|---|---|---|
| 1 | A0 → A1 → 1 animation chạy thật | Có kết quả nhìn thấy ngay, giữ động lực |
| 2 | A2 + A3 | Đóng gói pack |
| 3 | B0 + B1 | Chưa cần S31 |
| 4–5 | B2 (FAP) | Phụ thuộc kết quả A0 |
| 6 | C | Song song được nếu rảnh |
| sau | B3 (S31) | Khi đã mua và nhận board |

## 5. Rủi ro & kiểm thử (kiểu QA)

| Rủi ro | Giảm thiểu |
|---|---|
| Không biết firmware đang chạy | A0 làm đầu tiên; mọi bước sau đều ghi phiên bản |
| Nạp pack/firmware làm hỏng SD hoặc máy | Sao lưu SD; giữ pack mặc định; luôn có đường quay lại bản chính thức |
| Cấp nguồn board ESP từ Flipper làm sụt áp, reset | Nguồn riêng, chỉ nối mass + UART |
| FAP không khớp phiên bản firmware | Build bằng `ufbt` đúng bản; ghi phiên bản vào README |
| Giao thức UART mất/đứt gói | Mỗi gói một dòng + giới hạn 256 byte + timeout; kiểm tra 100 dòng liên tiếp |
| Lệnh giả từ cổng UART | Whitelist, không có lệnh nguy hiểm |

Bảng test tay cho mỗi nghiệm thu: `Bước / Kỳ vọng / Kết quả / Ảnh chụp`.

## 6. Cần điền (hiện đang trống)

| Mục | Giá trị |
|---|---|
| Firmware Flipper đang chạy + phiên bản | _chưa biết_ |
| Thẻ SD (dung lượng, đã format?) | _chưa rõ_ |
| Board dùng cho Track B | _T-Display S3 / DevKitC / S31 chưa chọn_ |

## 7. Lỗi/thiếu phát hiện khi kiểm tra `PLAN_MULTITOOL_VI.md` (chưa sửa file gốc)

| # | Vấn đề | Căn cứ trong repo | Đề xuất |
|---|---|---|---|
| 1 | Lệnh build mục 6 sai: `idf.py bmgr -b esp_claude_multitool && idf.py build` | `CLAUDE.md` và CI dùng `cd application/edge_agent` rồi `idf.py bmgr -c ./boards/<brand> -b <board>` | Sửa theo lệnh của CI |
| 2 | P3 (CC1101, nRF24) cần SPI nhưng repo **không có** Lua driver SPI chung (chỉ adc, gpio, i2c, ledc, mcpwm, pcnt, rmt, touch, uart) | `components/lua_modules/` | Thêm mục "lua_driver_spi" vào P2 làm tiền đề của P3 |
| 3 | P1 "Wi-Fi Analyzer" ghi "một phần (cmd_wifi)", nhưng `cmd_wifi` là lệnh console, **không có** module Lua quét Wi-Fi | `application/edge_agent/components/cmd_wifi`, không có `lua_module_wifi` | Thêm module Lua quét AP vào P1 |
| 4 | Host đề xuất `lilygo_t_display_s3` **không có trong ma trận CI** (CI chỉ build AMOLED 2.16" và DevKitC-1) | `.github/workflows/build-esp-claude.yml` | P0 phải thêm cả board def mới lẫn host vào matrix |
| 5 | Launcher "Tools" làm bằng App Lua, nhưng Launcher là System UI, App chỉ là gói con hiển thị trong đó | `app-package-spec.md` | Xác nhận trước: làm App đứng riêng hay đổi giao diện Launcher |
| 6 | Trích dẫn pháp luật (Thông tư 08/2021/TT-BTTTT, Điều 289 BLHS) chưa kiểm chứng | — | Tra lại văn bản gốc, ghi số điều/ngày hiệu lực |
| 7 | Chưa nhắc tới Flipper như một thiết bị cùng hệ | — | Đã bổ sung bằng file này |
