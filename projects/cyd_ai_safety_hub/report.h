// Tạo văn bản báo cáo trạng thái (tiếng Việt có dấu) cho Telegram, AI và trang web.
#pragma once
#include <Arduino.h>

#include "app_state.h"

namespace report {

// Báo cáo đầy đủ cho lệnh /status
String statusText(const Snapshot &s, const safety::Thresholds &th);
// Báo cáo ngắn gọn để đính kèm vào câu hỏi gửi AI
String aiContext(const Snapshot &s, const safety::Thresholds &th);
// Tin nhắn cảnh báo khi mức nguy cơ thay đổi
String alertText(const Snapshot &s, safety::Level prevLevel);
// Ngưỡng hiện tại
String thresholdsText(const safety::Thresholds &th);
// Thời gian hiện tại dạng "Thứ hai 26/10/2026 14:32" (rỗng nếu chưa đồng bộ giờ)
String nowText();
// Định dạng thời lượng ms -> "1 giờ 15 phút"
String durationText(uint32_t ms);

}  // namespace report
