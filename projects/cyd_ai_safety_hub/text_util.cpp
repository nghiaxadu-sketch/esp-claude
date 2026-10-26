#include "text_util.h"

#include <stdint.h>

#include "fold_table.h"

namespace textutil {

namespace {

// Giải mã 1 ký tự UTF-8 tại vị trí i. Trả về số byte đã đọc (>=1); cp = 0xFFFD nếu lỗi.
size_t decodeUtf8(const std::string &s, size_t i, uint32_t &cp) {
    const uint8_t c = static_cast<uint8_t>(s[i]);
    size_t len = 1;
    if (c < 0x80) {
        cp = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        cp = c & 0x1F;
        len = 2;
    } else if ((c & 0xF0) == 0xE0) {
        cp = c & 0x0F;
        len = 3;
    } else if ((c & 0xF8) == 0xF0) {
        cp = c & 0x07;
        len = 4;
    } else {
        cp = 0xFFFD;
        return 1;
    }
    if (i + len > s.size()) {
        cp = 0xFFFD;
        return 1;
    }
    for (size_t k = 1; k < len; ++k) {
        const uint8_t cc = static_cast<uint8_t>(s[i + k]);
        if ((cc & 0xC0) != 0x80) {
            cp = 0xFFFD;
            return 1;
        }
        cp = (cp << 6) | (cc & 0x3F);
    }
    return len;
}

char foldCodepoint(uint32_t cp) {
    size_t lo = 0;
    size_t hi = sizeof(kFoldTable) / sizeof(kFoldTable[0]);
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (kFoldTable[mid].cp == cp) return kFoldTable[mid].base;
        if (kFoldTable[mid].cp < cp) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    switch (cp) {
        case 0x2018:
        case 0x2019:
            return '\'';
        case 0x201C:
        case 0x201D:
            return '"';
        case 0x2013:
        case 0x2014:
            return '-';
        case 0x00B0:  // dấu độ
            return 'o';
        case 0x00A0:
            return ' ';
        default:
            return '?';
    }
}

// Lùi về đầu ký tự UTF-8 gần nhất (không nằm giữa chuỗi byte tiếp nối).
size_t safeCut(const std::string &s, size_t pos) {
    if (pos >= s.size()) return s.size();
    while (pos > 0 && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) --pos;
    return pos;
}

}  // namespace

std::string foldToAscii(const std::string &utf8) {
    std::string out;
    out.reserve(utf8.size());
    size_t i = 0;
    while (i < utf8.size()) {
        uint32_t cp = 0;
        const size_t n = decodeUtf8(utf8, i, cp);
        i += n;
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp == 0x2026) {
            out += "...";
        } else if (cp >= 0x1F000 || (cp >= 0x2600 && cp <= 0x27BF) || cp == 0xFE0F || cp == 0x200D) {
            // Emoji: bỏ qua trên màn hình
        } else {
            out.push_back(foldCodepoint(cp));
        }
    }
    return out;
}

std::vector<std::string> splitUtf8(const std::string &utf8, size_t maxBytes) {
    std::vector<std::string> parts;
    if (maxBytes < 8) maxBytes = 8;
    size_t start = 0;
    while (start < utf8.size()) {
        if (utf8.size() - start <= maxBytes) {
            parts.push_back(utf8.substr(start));
            break;
        }
        size_t cut = safeCut(utf8, start + maxBytes);
        // Ưu tiên cắt ở xuống dòng, rồi dấu cách, trong nửa sau của đoạn
        const size_t minCut = start + maxBytes / 2;
        size_t nl = utf8.rfind('\n', cut - 1);
        if (nl != std::string::npos && nl >= minCut) {
            cut = nl + 1;
        } else {
            size_t sp = utf8.rfind(' ', cut - 1);
            if (sp != std::string::npos && sp >= minCut) cut = sp + 1;
        }
        if (cut <= start) cut = safeCut(utf8, start + maxBytes);
        if (cut <= start) cut = start + maxBytes;  // chuỗi lỗi: cắt cứng
        parts.push_back(utf8.substr(start, cut - start));
        start = cut;
    }
    return parts;
}

std::string truncateUtf8(const std::string &utf8, size_t maxBytes) {
    if (utf8.size() <= maxBytes) return utf8;
    static const char kEllipsis[] = "\xE2\x80\xA6";  // …
    const size_t room = maxBytes > 3 ? maxBytes - 3 : 0;
    return utf8.substr(0, safeCut(utf8, room)) + kEllipsis;
}

std::vector<std::string> wrapAscii(const std::string &ascii, size_t cols, size_t maxLines) {
    std::vector<std::string> lines;
    if (cols == 0 || maxLines == 0) return lines;
    std::string cur;
    size_t i = 0;
    auto push = [&](const std::string &l) {
        if (lines.size() < maxLines) lines.push_back(l);
    };
    while (i < ascii.size() && lines.size() < maxLines) {
        if (ascii[i] == '\n') {
            push(cur);
            cur.clear();
            ++i;
            continue;
        }
        // Lấy 1 từ
        size_t j = i;
        while (j < ascii.size() && ascii[j] != ' ' && ascii[j] != '\n') ++j;
        std::string word = ascii.substr(i, j - i);
        while (word.size() > cols) {  // từ quá dài: cắt cứng
            if (!cur.empty()) {
                push(cur);
                cur.clear();
            }
            push(word.substr(0, cols));
            word = word.substr(cols);
        }
        if (cur.empty()) {
            cur = word;
        } else if (cur.size() + 1 + word.size() <= cols) {
            cur += ' ';
            cur += word;
        } else {
            push(cur);
            cur = word;
        }
        i = j;
        if (i < ascii.size() && ascii[i] == ' ') ++i;
    }
    if (!cur.empty()) push(cur);
    return lines;
}

}  // namespace textutil
