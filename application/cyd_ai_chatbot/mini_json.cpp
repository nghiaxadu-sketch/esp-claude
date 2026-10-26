#include "mini_json.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace mini_json {
namespace {

constexpr int kMaxDepth = 24;

class Parser {
public:
    explicit Parser(const std::string &text) : s_(text) {}

    bool parseDocument(Value &out, std::string &error)
    {
        skipSpace();
        if (!parseValue(out, 0)) {
            error = error_;
            return false;
        }
        skipSpace();
        if (pos_ != s_.size()) {
            fail("thua ky tu sau JSON");
            error = error_;
            return false;
        }
        return true;
    }

private:
    bool fail(const char *what)
    {
        if (error_.empty()) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "JSON loi o ky tu %u: %s", (unsigned)pos_, what);
            error_ = buf;
        }
        return false;
    }

    void skipSpace()
    {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\n' || s_[pos_] == '\r' || s_[pos_] == '\t')) {
            ++pos_;
        }
    }

    bool consume(char c)
    {
        skipSpace();
        if (pos_ < s_.size() && s_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool matchWord(const char *word)
    {
        size_t n = 0;
        while (word[n] != '\0') {
            if (pos_ + n >= s_.size() || s_[pos_ + n] != word[n]) {
                return false;
            }
            ++n;
        }
        pos_ += n;
        return true;
    }

    bool parseValue(Value &out, int depth)
    {
        if (depth > kMaxDepth) {
            return fail("long qua sau");
        }
        skipSpace();
        if (pos_ >= s_.size()) {
            return fail("het du lieu");
        }
        const char c = s_[pos_];
        if (c == '{') {
            return parseObject(out, depth);
        }
        if (c == '[') {
            return parseArray(out, depth);
        }
        if (c == '"') {
            out.type = Value::Type::String;
            return parseString(out.text);
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return parseNumber(out);
        }
        if (matchWord("true")) {
            out.type = Value::Type::Bool;
            out.boolean = true;
            return true;
        }
        if (matchWord("false")) {
            out.type = Value::Type::Bool;
            out.boolean = false;
            return true;
        }
        if (matchWord("null")) {
            out.type = Value::Type::Null;
            return true;
        }
        return fail("gia tri khong hop le");
    }

    bool parseObject(Value &out, int depth)
    {
        out.type = Value::Type::Object;
        ++pos_;  // '{'
        if (consume('}')) {
            return true;
        }
        while (true) {
            skipSpace();
            if (pos_ >= s_.size() || s_[pos_] != '"') {
                return fail("thieu ten truong");
            }
            std::string key;
            if (!parseString(key)) {
                return false;
            }
            if (!consume(':')) {
                return fail("thieu ':'");
            }
            out.keys.push_back(key);
            out.items.emplace_back();
            if (!parseValue(out.items.back(), depth + 1)) {
                return false;
            }
            if (consume(',')) {
                continue;
            }
            if (consume('}')) {
                return true;
            }
            return fail("thieu ',' hoac '}'");
        }
    }

    bool parseArray(Value &out, int depth)
    {
        out.type = Value::Type::Array;
        ++pos_;  // '['
        if (consume(']')) {
            return true;
        }
        while (true) {
            out.items.emplace_back();
            if (!parseValue(out.items.back(), depth + 1)) {
                return false;
            }
            if (consume(',')) {
                continue;
            }
            if (consume(']')) {
                return true;
            }
            return fail("thieu ',' hoac ']'");
        }
    }

    bool parseNumber(Value &out)
    {
        const char *start = s_.c_str() + pos_;
        char *end = nullptr;
        out.number = std::strtod(start, &end);
        if (end == start) {
            return fail("so khong hop le");
        }
        out.type = Value::Type::Number;
        pos_ += (size_t)(end - start);
        return true;
    }

    static void appendUtf8(uint32_t cp, std::string &out)
    {
        if (cp < 0x80) {
            out += (char)cp;
        } else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }

    bool parseHex4(uint32_t &cp)
    {
        if (pos_ + 4 > s_.size()) {
            return fail("\\u thieu chu so");
        }
        cp = 0;
        for (int i = 0; i < 4; ++i) {
            const char h = s_[pos_++];
            cp <<= 4;
            if (h >= '0' && h <= '9') {
                cp |= (uint32_t)(h - '0');
            } else if (h >= 'a' && h <= 'f') {
                cp |= (uint32_t)(h - 'a' + 10);
            } else if (h >= 'A' && h <= 'F') {
                cp |= (uint32_t)(h - 'A' + 10);
            } else {
                return fail("\\u sai");
            }
        }
        return true;
    }

    bool parseString(std::string &out)
    {
        ++pos_;  // opening quote
        out.clear();
        while (pos_ < s_.size()) {
            const char c = s_[pos_++];
            if (c == '"') {
                return true;
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= s_.size()) {
                break;
            }
            const char e = s_[pos_++];
            switch (e) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                uint32_t cp = 0;
                if (!parseHex4(cp)) {
                    return false;
                }
                // Surrogate pair (emoji); the display drops them anyway but keep the text valid.
                if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 6 <= s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                    pos_ += 2;
                    uint32_t low = 0;
                    if (!parseHex4(low)) {
                        return false;
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                appendUtf8(cp, out);
                break;
            }
            default: out += e; break;  // \" \\ \/
            }
        }
        return fail("chuoi chua dong '\"'");
    }

    const std::string &s_;
    size_t pos_ = 0;
    std::string error_;
};

}  // namespace

const Value *Value::get(const std::string &key) const
{
    if (type != Type::Object) {
        return nullptr;
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == key) {
            return &items[i];
        }
    }
    return nullptr;
}

std::string Value::getString(const std::string &key, const std::string &fallback) const
{
    const Value *v = get(key);
    return v != nullptr && v->isString() ? v->text : fallback;
}

double Value::getNumber(const std::string &key, double fallback) const
{
    const Value *v = get(key);
    return v != nullptr && v->isNumber() ? v->number : fallback;
}

bool parse(const std::string &text, Value &out, std::string &error)
{
    out = Value();
    Parser parser(text);
    return parser.parseDocument(out, error);
}

std::string extractObject(const std::string &text)
{
    const size_t start = text.find('{');
    if (start == std::string::npos) {
        return std::string();
    }
    // Walk to the matching brace, ignoring braces inside strings.
    int depth = 0;
    bool inString = false;
    for (size_t i = start; i < text.size(); ++i) {
        const char c = text[i];
        if (inString) {
            if (c == '\\') {
                ++i;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}' && --depth == 0) {
            return text.substr(start, i - start + 1);
        }
    }
    return std::string();
}

std::string quote(const std::string &text)
{
    std::string out = "\"";
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if ((unsigned char)c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(unsigned char)c);
                out += buf;
            } else {
                out += c;
            }
        }
    }
    out += '"';
    return out;
}

}  // namespace mini_json
