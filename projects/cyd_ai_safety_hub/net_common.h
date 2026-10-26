// Tiện ích mạng dùng chung: client HTTPS có xác thực chứng chỉ, kiểm tra Wi-Fi.
#pragma once
#include <Arduino.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

namespace net {

// Gắn bộ chứng chỉ gốc vào client (mỗi task dùng 1 client riêng)
void setupSecureClient(NetworkClientSecure &client);

inline bool online() { return WiFi.status() == WL_CONNECTED; }

// Mã hóa URL (cho tham số query)
String urlEncode(const String &s);

// Ổ cắm thông minh (Tasmota/Shelly/... có URL bật/tắt qua HTTP). Trả về true nếu HTTP 2xx.
bool callPlugUrl(const String &url);

}  // namespace net
