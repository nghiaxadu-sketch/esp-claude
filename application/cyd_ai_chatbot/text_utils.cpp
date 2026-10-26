#include "text_utils.h"

#include <cstdint>

namespace text_utils {
namespace {

// ASCII fallback for U+00C0..U+00FF (Latin-1 letters).
constexpr char kLatin1Map[] = "AAAAAAACEEEEIIII"
                              "DNOOOOOxOUUUUYPs"
                              "aaaaaaaceeeeiiii"
                              "dnooooo/ouuuuypy";
constexpr uint32_t kLatin1First = 0x00C0;
constexpr uint32_t kLatin1Last = 0x00FF;

// U+1EA0..U+1EF9 (Latin Extended Additional) holds all Vietnamese letters with tone marks,
// as upper/lower pairs (even = upper case, odd = lower case).
struct VietRange {
    uint32_t first;
    uint32_t last;
    char upper;
};

constexpr VietRange kVietRanges[] = {
    {0x1EA0, 0x1EB7, 'A'},
    {0x1EB8, 0x1EC7, 'E'},
    {0x1EC8, 0x1ECB, 'I'},
    {0x1ECC, 0x1EE3, 'O'},
    {0x1EE4, 0x1EF1, 'U'},
    {0x1EF2, 0x1EF9, 'Y'},
};

struct SingleMap {
    uint32_t codepoint;
    const char *ascii;
};

constexpr SingleMap kSingleMaps[] = {
    {0x0102, "A"}, {0x0103, "a"},   // Ă ă
    {0x0110, "D"}, {0x0111, "d"},   // Đ đ
    {0x0128, "I"}, {0x0129, "i"},   // Ĩ ĩ
    {0x0168, "U"}, {0x0169, "u"},   // Ũ ũ
    {0x01A0, "O"}, {0x01A1, "o"},   // Ơ ơ
    {0x01AF, "U"}, {0x01B0, "u"},   // Ư ư
    {0x00A0, " "},                  // no-break space
    {0x00B0, "o"},                  // degree sign
    {0x00AB, "\""}, {0x00BB, "\""}, // « »
    {0x2018, "'"}, {0x2019, "'"},
    {0x201C, "\""}, {0x201D, "\""},
    {0x2013, "-"}, {0x2014, "-"},
    {0x2022, "*"},
    {0x2026, "..."},
    {0x2192, "->"},
    {0x00D7, "x"},
};

constexpr uint32_t kCombiningFirst = 0x0300;  // decomposed tone marks
constexpr uint32_t kCombiningLast = 0x036F;
constexpr uint32_t kVariationSelectorFirst = 0xFE00;
constexpr uint32_t kVariationSelectorLast = 0xFE0F;
constexpr uint32_t kZeroWidthJoiner = 0x200D;
constexpr uint32_t kEmojiFirst = 0x1F000;
constexpr uint32_t kMiscSymbolsFirst = 0x2600;  // ☀ ✓ ✨ ...
constexpr uint32_t kMiscSymbolsLast = 0x27BF;
constexpr uint32_t kReplacement = 0xFFFD;

// Decodes one UTF-8 sequence starting at text[i]; advances i. Invalid bytes yield kReplacement.
uint32_t decodeUtf8(const std::string &text, size_t &i)
{
    const uint8_t lead = static_cast<uint8_t>(text[i++]);
    if (lead < 0x80) {
        return lead;
    }
    int extra = 0;
    uint32_t cp = 0;
    if ((lead & 0xE0) == 0xC0) {
        extra = 1;
        cp = lead & 0x1F;
    } else if ((lead & 0xF0) == 0xE0) {
        extra = 2;
        cp = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
        extra = 3;
        cp = lead & 0x07;
    } else {
        return kReplacement;
    }
    for (int n = 0; n < extra; ++n) {
        if (i >= text.size() || (static_cast<uint8_t>(text[i]) & 0xC0) != 0x80) {
            return kReplacement;
        }
        cp = (cp << 6) | (static_cast<uint8_t>(text[i++]) & 0x3F);
    }
    return cp;
}

void appendAscii(uint32_t cp, std::string &out)
{
    if (cp == '\t') {
        out += ' ';
        return;
    }
    if (cp == '\n' || (cp >= 0x20 && cp < 0x7F)) {
        out += static_cast<char>(cp);
        return;
    }
    if (cp < 0x20 || cp == 0x7F) {
        return;  // other control characters (\r ...)
    }
    if (cp >= kLatin1First && cp <= kLatin1Last && cp != 0x00D7 && cp != 0x00F7) {
        out += kLatin1Map[cp - kLatin1First];
        return;
    }
    for (const VietRange &r : kVietRanges) {
        if (cp >= r.first && cp <= r.last) {
            const bool lower = ((cp - r.first) & 1U) != 0;
            out += lower ? static_cast<char>(r.upper - 'A' + 'a') : r.upper;
            return;
        }
    }
    for (const SingleMap &m : kSingleMaps) {
        if (m.codepoint == cp) {
            out += m.ascii;
            return;
        }
    }
    if (cp == 0x00F7) {
        out += '/';
        return;
    }
    const bool invisible = (cp >= kCombiningFirst && cp <= kCombiningLast) ||
                           (cp >= kVariationSelectorFirst && cp <= kVariationSelectorLast) ||
                           cp == kZeroWidthJoiner;
    const bool pictograph = cp >= kEmojiFirst || (cp >= kMiscSymbolsFirst && cp <= kMiscSymbolsLast);
    if (invisible || pictograph) {
        return;
    }
    out += '?';
}

}  // namespace

std::string utf8ToAscii(const std::string &utf8)
{
    std::string out;
    out.reserve(utf8.size());
    size_t i = 0;
    while (i < utf8.size()) {
        appendAscii(decodeUtf8(utf8, i), out);
    }
    return out;
}

std::string stripMarkdown(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    bool lineStart = true;
    size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (lineStart && c == '#') {
            while (i < text.size() && text[i] == '#') {
                ++i;
            }
            while (i < text.size() && text[i] == ' ') {
                ++i;
            }
            lineStart = false;
            continue;
        }
        if ((c == '*' || c == '_') && i + 1 < text.size() && text[i + 1] == c) {
            i += 2;
            continue;
        }
        if (c == '`') {
            ++i;
            continue;
        }
        out += c;
        lineStart = (c == '\n');
        ++i;
    }
    return out;
}

std::vector<std::string> wrapText(const std::string &text, size_t maxChars)
{
    std::vector<std::string> lines;
    if (maxChars == 0) {
        return lines;
    }
    size_t paraStart = 0;
    while (paraStart <= text.size()) {
        size_t paraEnd = text.find('\n', paraStart);
        if (paraEnd == std::string::npos) {
            paraEnd = text.size();
        }
        const std::string para = text.substr(paraStart, paraEnd - paraStart);

        std::string line;
        size_t pos = 0;
        while (pos < para.size()) {
            while (pos < para.size() && para[pos] == ' ') {
                ++pos;
            }
            if (pos >= para.size()) {
                break;
            }
            size_t wordEnd = para.find(' ', pos);
            if (wordEnd == std::string::npos) {
                wordEnd = para.size();
            }
            std::string word = para.substr(pos, wordEnd - pos);
            pos = wordEnd;

            const size_t needed = line.empty() ? word.size() : line.size() + 1 + word.size();
            if (needed <= maxChars) {
                if (!line.empty()) {
                    line += ' ';
                }
                line += word;
                continue;
            }
            if (!line.empty()) {
                lines.push_back(line);
                line.clear();
            }
            while (word.size() > maxChars) {
                lines.push_back(word.substr(0, maxChars));
                word.erase(0, maxChars);
            }
            line = word;
        }
        lines.push_back(line);
        if (paraEnd == text.size()) {
            break;
        }
        paraStart = paraEnd + 1;
    }
    return lines;
}

}  // namespace text_utils
