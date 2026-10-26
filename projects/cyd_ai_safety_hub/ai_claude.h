// Trợ lý AI dùng Claude (Anthropic Messages API, gọi HTTP trực tiếp từ ESP32).
// - Trò chuyện tự do như chatbot (hỏi đáp, dịch, viết, tính toán, tra cứu web bằng web_search)
// - Biết số liệu cảm biến hiện tại (đính kèm vào mỗi câu hỏi)
// - Có công cụ điều khiển thiết bị: đổi ngưỡng, tắt còi, bật/tắt ổ cắm sạc, xem lịch sử 3 giờ
// Chạy trong task riêng, xử lý lần lượt từng câu hỏi.
#pragma once
#include <Arduino.h>

namespace ai {

enum Source : uint8_t { SRC_TELEGRAM = 0, SRC_WEB = 1 };

void begin();
bool configured();
// Gửi câu hỏi. Với SRC_WEB, outId nhận mã để hỏi kết quả qua webResult().
bool submit(Source src, int64_t chatId, const String &userName, const String &text, uint32_t *outId);
// Lấy kết quả cho trang web. Trả về false nếu mã không tồn tại.
bool webResult(uint32_t id, String &reply, bool &done);
void resetHistory();

}  // namespace ai
