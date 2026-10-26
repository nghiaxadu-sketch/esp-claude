// Bot Telegram: nhận lệnh/câu hỏi từ điện thoại và gửi cảnh báo về điện thoại.
// Chạy trong task riêng (nhân 0) để không làm chậm việc đo và còi báo động.
#pragma once
#include <Arduino.h>
#include <NetworkClientSecure.h>

namespace telegram {

void begin();
// Gửi cảnh báo tới mọi chat đã ghép đôi (không chặn, xếp hàng)
bool broadcast(const String &text);
// Gửi tin nhắn tới 1 chat bằng client của task gọi (dùng trong task AI)
bool sendMessage(NetworkClientSecure &client, int64_t chatId, const String &text);
// Gửi trạng thái "đang soạn tin..." (typing)
void sendTyping(NetworkClientSecure &client, int64_t chatId);
// Yêu cầu ngắt ổ cắm thông minh (thực hiện trong task mạng)
void requestPlugOff();

}  // namespace telegram
