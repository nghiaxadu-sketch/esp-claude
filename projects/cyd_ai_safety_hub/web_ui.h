// Máy chủ web trong mạng LAN: http://<IP thiết bị>/ (đăng nhập: admin / mã PIN trên màn hình)
#pragma once

namespace webui {

void begin();
void handle();  // gọi trong loop()

}  // namespace webui
