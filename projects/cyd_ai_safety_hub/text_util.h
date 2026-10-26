// Tiện ích xử lý chuỗi UTF-8 (thuần C++, không phụ thuộc Arduino -> test được trên máy tính).
#pragma once
#include <stddef.h>
#include <string>
#include <vector>

namespace textutil {

// Bỏ dấu tiếng Việt để hiển thị trên TFT (font ASCII). Ký tự không hỗ trợ -> '?'.
std::string foldToAscii(const std::string &utf8);

// Cắt chuỗi UTF-8 thành các đoạn <= maxBytes, không cắt giữa ký tự nhiều byte,
// ưu tiên cắt ở xuống dòng hoặc dấu cách. Dùng để gửi Telegram (giới hạn 4096 ký tự).
std::vector<std::string> splitUtf8(const std::string &utf8, size_t maxBytes);

// Cắt bớt chuỗi UTF-8 về tối đa maxBytes (an toàn ranh giới ký tự), thêm "…" nếu bị cắt.
std::string truncateUtf8(const std::string &utf8, size_t maxBytes);

// Ngắt dòng cho chuỗi ASCII theo số ký tự mỗi dòng (dùng cho màn TFT).
std::vector<std::string> wrapAscii(const std::string &ascii, size_t cols, size_t maxLines);

}  // namespace textutil
