#include "mini_script.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mini_script {
namespace {

constexpr int kMaxDepth = 48;
constexpr size_t kMaxStringLen = 2048;
constexpr double kEpsilon = 1e-9;
constexpr double kMaxIntegerPrint = 1e15;
constexpr size_t kErrorContextChars = 24;
constexpr size_t kMaxArgs = 16;

struct BuiltinInfo {
    const char *name;
    uint8_t minArgs;
    uint8_t maxArgs;
};

constexpr uint8_t kManyArgs = kMaxArgs;

constexpr BuiltinInfo kBuiltins[] = {
    {"abs", 1, 1},     {"min", 1, kManyArgs}, {"max", 1, kManyArgs}, {"floor", 1, 1},   {"ceil", 1, 1},
    {"round", 1, 2},   {"int", 1, 1},         {"sqrt", 1, 1},        {"pow", 2, 2},     {"sin", 1, 1},
    {"cos", 1, 1},     {"clamp", 3, 3},       {"rand", 2, 2},        {"str", 1, 1},     {"num", 1, 1},
    {"len", 1, 1},     {"sub", 2, 3},         {"upper", 1, 1},       {"lower", 1, 1},   {"pad", 2, 2},
    {"fmt", 2, 2},     {"mmss", 1, 1},        {"hhmmss", 1, 1},      {"pick", 2, kManyArgs},
    {"count", 2, 2},   {"part", 3, 3},        {"contains", 2, 2},    {"repeat", 2, 2},
};

std::string truncated(std::string s)
{
    if (s.size() > kMaxStringLen) {
        s.resize(kMaxStringLen);
    }
    return s;
}

bool valuesEqual(const Value &a, const Value &b)
{
    if (!a.isText && !b.isText) {
        return std::fabs(a.num - b.num) < kEpsilon;
    }
    return a.toString() == b.toString();
}

std::string asciiCase(std::string s, bool upper)
{
    for (char &c : s) {
        if (upper && c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        } else if (!upper && c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
    }
    return s;
}

// Splits by separator; an empty separator gives single characters.
std::vector<std::string> splitText(const std::string &s, const std::string &sep)
{
    std::vector<std::string> parts;
    if (sep.empty()) {
        for (char c : s) {
            parts.push_back(std::string(1, c));
        }
        return parts;
    }
    size_t start = 0;
    while (true) {
        const size_t at = s.find(sep, start);
        if (at == std::string::npos) {
            parts.push_back(s.substr(start));
            return parts;
        }
        parts.push_back(s.substr(start, at - start));
        start = at + sep.size();
    }
}

std::string twoDigits(long v)
{
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%02ld", v);
    return buf;
}

}  // namespace

// ---------------------------------------------------------------- Value

Value Value::number(double n)
{
    Value v;
    v.num = std::isfinite(n) ? n : 0;
    return v;
}

Value Value::string(const std::string &s)
{
    Value v;
    v.isText = true;
    v.text = truncated(s);
    return v;
}

double Value::toNumber() const
{
    if (!isText) {
        return num;
    }
    const char *start = text.c_str();
    char *end = nullptr;
    const double n = std::strtod(start, &end);
    return end == start ? 0 : n;
}

std::string Value::toString() const
{
    return isText ? text : formatNumber(num);
}

bool Value::truthy() const
{
    return isText ? !text.empty() : std::fabs(num) > kEpsilon;
}

std::string formatNumber(double n)
{
    if (!std::isfinite(n)) {
        return "0";
    }
    char buf[40];
    if (std::fabs(n - std::round(n)) < kEpsilon && std::fabs(n) < kMaxIntegerPrint) {
        std::snprintf(buf, sizeof(buf), "%lld", (long long)std::llround(n));
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "%.2f", n);
    std::string s = buf;
    while (!s.empty() && s.back() == '0') {
        s.pop_back();
    }
    if (!s.empty() && s.back() == '.') {
        s.pop_back();
    }
    return s;
}

// ---------------------------------------------------------------- parser + evaluator

// Parses and evaluates in one pass. When `skip_` is set the code is parsed without side effects
// (untaken branches, short-circuit operands, and the whole program in check mode).
class Exec {
public:
    Exec(Interpreter &in, const std::string &src, bool checking)
        : in_(in), s_(src), checking_(checking), skip_(checking)
    {
    }

    bool program()
    {
        while (true) {
            ws();
            if (atEnd()) {
                return true;
            }
            if (peek() == '}') {
                return fail("thua dau '}'");
            }
            if (!statement()) {
                return false;
            }
        }
    }

    bool expressionOnly(Value &out)
    {
        if (!expr(out, 0)) {
            return false;
        }
        ws();
        return atEnd() || fail("thua ky tu sau bieu thuc");
    }

    const std::string &error() const { return error_; }

private:
    // ---- lexing helpers
    bool atEnd() const { return pos_ >= s_.size(); }
    char peek(size_t ahead = 0) const { return pos_ + ahead < s_.size() ? s_[pos_ + ahead] : '\0'; }

    static bool isIdentStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    static bool isIdentChar(char c) { return isIdentStart(c) || (c >= '0' && c <= '9'); }

    void ws()
    {
        while (!atEnd()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else if (c == '/' && peek(1) == '/') {
                while (!atEnd() && peek() != '\n') {
                    ++pos_;
                }
            } else {
                return;
            }
        }
    }

    bool match(const char *op)
    {
        ws();
        size_t n = 0;
        while (op[n] != '\0') {
            if (peek(n) != op[n]) {
                return false;
            }
            ++n;
        }
        pos_ += n;
        return true;
    }

    // Reads an identifier at the cursor without consuming it when `consume` is false.
    bool readIdent(std::string &name, bool consume = true)
    {
        ws();
        if (!isIdentStart(peek())) {
            return false;
        }
        size_t end = pos_;
        while (end < s_.size() && isIdentChar(s_[end])) {
            ++end;
        }
        name = s_.substr(pos_, end - pos_);
        if (consume) {
            pos_ = end;
        }
        return true;
    }

    bool keyword(const char *word)
    {
        std::string name;
        const size_t save = pos_;
        if (readIdent(name) && name == word) {
            return true;
        }
        pos_ = save;
        return false;
    }

    bool fail(const std::string &what)
    {
        if (error_.empty()) {
            const size_t from = pos_ > kErrorContextChars / 2 ? pos_ - kErrorContextChars / 2 : 0;
            error_ = what + " (vi tri " + std::to_string(pos_) + ", gan: \"" + s_.substr(from, kErrorContextChars) + "\")";
        }
        return false;
    }

    // ---- statements
    bool statement()
    {
        ws();
        if (match(";")) {
            return true;
        }
        if (keyword("if")) {
            return ifStatement();
        }
        const size_t start = pos_;
        std::string name;
        if (readIdent(name)) {
            if (name == "else") {
                return fail("'else' khong co 'if'");
            }
            if (assignment(name)) {
                return error_.empty();
            }
            if (!error_.empty()) {
                return false;
            }
            pos_ = start;
        }
        Value ignored;
        return expr(ignored, 0);
    }

    // Returns true when the text after `name` was an assignment (and consumed it).
    bool assignment(const std::string &name)
    {
        ws();
        char op = 0;
        if (peek() == '=' && peek(1) != '=') {
            op = '=';
            pos_ += 1;
        } else if ((peek() == '+' || peek() == '-' || peek() == '*' || peek() == '/' || peek() == '%') &&
                   peek(1) == '=') {
            op = peek();
            pos_ += 2;
        } else if ((peek() == '+' && peek(1) == '+') || (peek() == '-' && peek(1) == '-')) {
            op = peek() == '+' ? 'i' : 'd';
            pos_ += 2;
        } else {
            return false;
        }
        if (isReserved(name)) {
            fail("khong gan duoc cho '" + name + "'");
            return true;
        }
        Value rhs = Value::number(1);
        if (op != 'i' && op != 'd' && !expr(rhs, 0)) {
            return true;
        }
        if (checking_ && op != '=' && !in_.hasVar(name)) {
            fail("bien '" + name + "' chua khai bao trong \"vars\"");
            return true;
        }
        if (skip_) {
            return true;
        }
        if (op == '=') {
            in_.setVar(name, rhs);
            return true;
        }
        Value current;
        if (!in_.getVar(name, current)) {
            fail("bien '" + name + "' chua co gia tri");
            return true;
        }
        Value next;
        switch (op) {
        case '+':
            next = (current.isText || rhs.isText) ? Value::string(current.toString() + rhs.toString())
                                                  : Value::number(current.num + rhs.num);
            break;
        case '-': next = Value::number(current.toNumber() - rhs.toNumber()); break;
        case '*': next = Value::number(current.toNumber() * rhs.toNumber()); break;
        case '/': next = Value::number(divide(current.toNumber(), rhs.toNumber())); break;
        case '%': next = Value::number(modulo(current.toNumber(), rhs.toNumber())); break;
        case 'i': next = Value::number(current.toNumber() + 1); break;
        default: next = Value::number(current.toNumber() - 1); break;
        }
        in_.setVar(name, next);
        return true;
    }

    bool ifStatement()
    {
        if (!match("(")) {
            return fail("thieu '(' sau if");
        }
        Value cond;
        if (!expr(cond, 0)) {
            return false;
        }
        if (!match(")")) {
            return fail("thieu ')' sau dieu kien if");
        }
        const bool outerSkip = skip_;
        const bool taken = !outerSkip && cond.truthy();
        skip_ = outerSkip || !taken;
        if (!body()) {
            return false;
        }
        skip_ = outerSkip;
        if (keyword("else")) {
            skip_ = outerSkip || taken;
            const bool ok = keyword("if") ? ifStatement() : body();
            skip_ = outerSkip;
            return ok;
        }
        return true;
    }

    bool body()
    {
        if (!match("{")) {
            return statement();
        }
        while (true) {
            ws();
            if (atEnd()) {
                return fail("thieu '}'");
            }
            if (match("}")) {
                return true;
            }
            if (!statement()) {
                return false;
            }
        }
    }

    // ---- expressions (precedence climbing by hand)
    bool expr(Value &out, int depth)
    {
        if (depth > kMaxDepth) {
            return fail("bieu thuc long qua sau");
        }
        if (!orExpr(out, depth + 1)) {
            return false;
        }
        if (!match("?")) {
            return true;
        }
        const bool outerSkip = skip_;
        const bool cond = out.truthy();
        Value a, b;
        skip_ = outerSkip || !cond;
        if (!expr(a, depth + 1)) {
            return false;
        }
        if (!match(":")) {
            return fail("thieu ':' trong bieu thuc ? :");
        }
        skip_ = outerSkip || cond;
        if (!expr(b, depth + 1)) {
            return false;
        }
        skip_ = outerSkip;
        out = cond ? a : b;
        return true;
    }

    bool orExpr(Value &out, int depth)
    {
        if (!andExpr(out, depth)) {
            return false;
        }
        while (match("||") || keyword("or")) {
            const bool outerSkip = skip_;
            const bool left = out.truthy();
            skip_ = outerSkip || left;
            Value right;
            if (!andExpr(right, depth)) {
                return false;
            }
            skip_ = outerSkip;
            out = Value::number(left || right.truthy() ? 1 : 0);
        }
        return true;
    }

    bool andExpr(Value &out, int depth)
    {
        if (!equality(out, depth)) {
            return false;
        }
        while (match("&&") || keyword("and")) {
            const bool outerSkip = skip_;
            const bool left = out.truthy();
            skip_ = outerSkip || !left;
            Value right;
            if (!equality(right, depth)) {
                return false;
            }
            skip_ = outerSkip;
            out = Value::number(left && right.truthy() ? 1 : 0);
        }
        return true;
    }

    bool equality(Value &out, int depth)
    {
        if (!relational(out, depth)) {
            return false;
        }
        while (true) {
            bool equal;
            if (match("==")) {
                equal = true;
            } else if (match("!=")) {
                equal = false;
            } else {
                return true;
            }
            Value right;
            if (!relational(right, depth)) {
                return false;
            }
            out = Value::number(valuesEqual(out, right) == equal ? 1 : 0);
        }
    }

    bool relational(Value &out, int depth)
    {
        if (!additive(out, depth)) {
            return false;
        }
        while (true) {
            const char *op = nullptr;
            for (const char *candidate : {"<=", ">=", "<", ">"}) {
                ws();
                if (match(candidate)) {
                    op = candidate;
                    break;
                }
            }
            if (op == nullptr) {
                return true;
            }
            Value right;
            if (!additive(right, depth)) {
                return false;
            }
            int cmp;
            if (out.isText && right.isText) {
                cmp = out.text.compare(right.text);
            } else {
                const double d = out.toNumber() - right.toNumber();
                cmp = std::fabs(d) < kEpsilon ? 0 : (d < 0 ? -1 : 1);
            }
            bool result;
            if (op[0] == '<') {
                result = op[1] == '=' ? cmp <= 0 : cmp < 0;
            } else {
                result = op[1] == '=' ? cmp >= 0 : cmp > 0;
            }
            out = Value::number(result ? 1 : 0);
        }
    }

    bool additive(Value &out, int depth)
    {
        if (!multiplicative(out, depth)) {
            return false;
        }
        while (true) {
            ws();
            const char op = peek();
            if ((op != '+' && op != '-') || peek(1) == '=' || peek(1) == op) {
                return true;
            }
            ++pos_;
            Value right;
            if (!multiplicative(right, depth)) {
                return false;
            }
            if (op == '+' && (out.isText || right.isText)) {
                out = Value::string(out.toString() + right.toString());
            } else {
                out = Value::number(op == '+' ? out.toNumber() + right.toNumber() : out.toNumber() - right.toNumber());
            }
        }
    }

    static double divide(double a, double b) { return std::fabs(b) < kEpsilon ? 0 : a / b; }
    static double modulo(double a, double b) { return std::fabs(b) < kEpsilon ? 0 : std::fmod(a, b); }

    bool multiplicative(Value &out, int depth)
    {
        if (!unary(out, depth)) {
            return false;
        }
        while (true) {
            ws();
            const char op = peek();
            if ((op != '*' && op != '/' && op != '%') || peek(1) == '=' || (op == '/' && peek(1) == '/')) {
                return true;
            }
            ++pos_;
            Value right;
            if (!unary(right, depth)) {
                return false;
            }
            const double a = out.toNumber(), b = right.toNumber();
            out = Value::number(op == '*' ? a * b : op == '/' ? divide(a, b) : modulo(a, b));
        }
    }

    bool unary(Value &out, int depth)
    {
        if (depth > kMaxDepth) {
            return fail("bieu thuc long qua sau");
        }
        ws();
        if (peek() == '!' && peek(1) != '=') {
            ++pos_;
            if (!unary(out, depth + 1)) {
                return false;
            }
            out = Value::number(out.truthy() ? 0 : 1);
            return true;
        }
        if (keyword("not")) {
            if (!unary(out, depth + 1)) {
                return false;
            }
            out = Value::number(out.truthy() ? 0 : 1);
            return true;
        }
        if (peek() == '-' || peek() == '+') {
            const char op = peek();
            ++pos_;
            if (!unary(out, depth + 1)) {
                return false;
            }
            out = Value::number(op == '-' ? -out.toNumber() : out.toNumber());
            return true;
        }
        return primary(out, depth);
    }

    static bool isReserved(const std::string &name)
    {
        return name == "if" || name == "else" || name == "true" || name == "false" || name == "and" ||
               name == "or" || name == "not";
    }

    bool primary(Value &out, int depth)
    {
        ws();
        const char c = peek();
        if (c == '(') {
            ++pos_;
            if (!expr(out, depth + 1)) {
                return false;
            }
            return match(")") || fail("thieu ')'");
        }
        if (c == '\'' || c == '"') {
            return stringLiteral(out);
        }
        if ((c >= '0' && c <= '9') || (c == '.' && peek(1) >= '0' && peek(1) <= '9')) {
            return numberLiteral(out);
        }
        std::string name;
        if (!readIdent(name)) {
            return fail(atEnd() ? "bieu thuc bi thieu" : std::string("ky tu la '") + c + "'");
        }
        if (name == "true" || name == "false") {
            out = Value::number(name == "true" ? 1 : 0);
            return true;
        }
        if (isReserved(name)) {
            return fail("khong dung duoc '" + name + "' o day");
        }
        if (match("(")) {
            return call(name, out, depth);
        }
        if (checking_ && !in_.hasVar(name)) {
            return fail("bien '" + name + "' chua khai bao trong \"vars\"");
        }
        if (skip_) {
            out = Value::number(0);
            return true;
        }
        if (!in_.getVar(name, out)) {
            return fail("bien '" + name + "' chua co gia tri");
        }
        return true;
    }

    bool stringLiteral(Value &out)
    {
        const char quote = s_[pos_++];
        std::string text;
        while (!atEnd()) {
            char c = s_[pos_++];
            if (c == quote) {
                out = Value::string(text);
                return true;
            }
            if (c == '\\' && !atEnd()) {
                c = s_[pos_++];
                if (c == 'n') {
                    c = '\n';
                }
            }
            text += c;
        }
        return fail("chuoi chua dong dau nhay");
    }

    bool numberLiteral(Value &out)
    {
        const char *start = s_.c_str() + pos_;
        char *end = nullptr;
        const double n = std::strtod(start, &end);
        if (end == start) {
            return fail("so khong hop le");
        }
        pos_ += (size_t)(end - start);
        out = Value::number(n);
        return true;
    }

    bool call(const std::string &name, Value &out, int depth)
    {
        std::vector<Value> args;
        if (!match(")")) {
            while (true) {
                Value arg;
                if (!expr(arg, depth + 1)) {
                    return false;
                }
                args.push_back(arg);
                if (args.size() > kMaxArgs) {
                    return fail("qua nhieu tham so");
                }
                if (match(")")) {
                    break;
                }
                if (!match(",")) {
                    return fail("thieu ',' hoac ')' khi goi " + name + "()");
                }
            }
        }
        const bool builtin = Interpreter::isBuiltin(name, args.size());
        const bool hosted = !builtin && in_.host_ != nullptr && in_.host_->hasFunction(name, args.size());
        if (!builtin && !hosted) {
            return fail("ham " + name + "() khong ton tai hoac sai so tham so (" + std::to_string(args.size()) + ")");
        }
        if (skip_) {
            out = Value::number(0);
            return true;
        }
        std::string err;
        const bool ok = builtin ? callBuiltin(name, args, out, err) : in_.host_->callFunction(name, args, out, err);
        return ok || fail(name + "(): " + err);
    }

    bool callBuiltin(const std::string &name, const std::vector<Value> &a, Value &out, std::string &err)
    {
        auto n = [&](size_t i) { return a[i].toNumber(); };
        auto s = [&](size_t i) { return a[i].toString(); };
        if (name == "abs") { out = Value::number(std::fabs(n(0))); }
        else if (name == "min" || name == "max") {
            double r = n(0);
            for (size_t i = 1; i < a.size(); ++i) {
                r = name == "min" ? std::fmin(r, n(i)) : std::fmax(r, n(i));
            }
            out = Value::number(r);
        }
        else if (name == "floor") { out = Value::number(std::floor(n(0))); }
        else if (name == "ceil") { out = Value::number(std::ceil(n(0))); }
        else if (name == "round") {
            const double scale = a.size() > 1 ? std::pow(10.0, n(1)) : 1.0;
            out = Value::number(std::round(n(0) * scale) / scale);
        }
        else if (name == "int") { out = Value::number(std::trunc(n(0))); }
        else if (name == "sqrt") { out = Value::number(n(0) > 0 ? std::sqrt(n(0)) : 0); }
        else if (name == "pow") { out = Value::number(std::pow(n(0), n(1))); }
        else if (name == "sin") { out = Value::number(std::sin(n(0))); }
        else if (name == "cos") { out = Value::number(std::cos(n(0))); }
        else if (name == "clamp") { out = Value::number(std::fmin(std::fmax(n(0), n(1)), n(2))); }
        else if (name == "rand") {
            const long lo = std::lround(std::fmin(n(0), n(1)));
            const long hi = std::lround(std::fmax(n(0), n(1)));
            out = Value::number((double)(lo + (long)(in_.nextRandom() % (uint32_t)(hi - lo + 1))));
        }
        else if (name == "str") { out = Value::string(s(0)); }
        else if (name == "num") { out = Value::number(n(0)); }
        else if (name == "len") { out = Value::number((double)s(0).size()); }
        else if (name == "sub") {
            const std::string text = s(0);
            const long start = std::lround(std::fmax(0, n(1)));
            const long count = a.size() > 2 ? std::lround(std::fmax(0, n(2))) : (long)text.size();
            out = Value::string((size_t)start < text.size() ? text.substr((size_t)start, (size_t)count) : "");
        }
        else if (name == "upper" || name == "lower") { out = Value::string(asciiCase(s(0), name == "upper")); }
        else if (name == "pad") {
            std::string text = formatNumber(std::trunc(n(0)));
            const size_t width = (size_t)std::lround(std::fmin(std::fmax(n(1), 0), 12));
            const bool negative = !text.empty() && text[0] == '-';
            if (negative) {
                text.erase(0, 1);
            }
            while (text.size() < width) {
                text.insert(0, "0");
            }
            out = Value::string(negative ? "-" + text : text);
        }
        else if (name == "fmt") {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%.*f", (int)std::lround(std::fmin(std::fmax(n(1), 0), 6)), n(0));
            out = Value::string(buf);
        }
        else if (name == "mmss" || name == "hhmmss") {
            long total = std::lround(std::fmax(0, n(0)));
            std::string text;
            if (name == "hhmmss") {
                text = std::to_string(total / 3600) + ":";
                total %= 3600;
                text += twoDigits(total / 60) + ":" + twoDigits(total % 60);
            } else {
                text = twoDigits(total / 60) + ":" + twoDigits(total % 60);
            }
            out = Value::string(text);
        }
        else if (name == "pick") {
            const long count = (long)a.size() - 1;
            long i = std::lround(n(0)) % count;
            if (i < 0) {
                i += count;
            }
            out = a[(size_t)i + 1];
        }
        else if (name == "count") { out = Value::number((double)splitText(s(0), s(1)).size()); }
        else if (name == "part") {
            const std::vector<std::string> parts = splitText(s(0), s(1));
            const long i = std::lround(n(2));
            out = Value::string(i >= 0 && (size_t)i < parts.size() ? parts[(size_t)i] : "");
        }
        else if (name == "contains") { out = Value::number(s(0).find(s(1)) != std::string::npos ? 1 : 0); }
        else if (name == "repeat") {
            std::string text;
            const long times = std::lround(std::fmin(std::fmax(n(1), 0), (double)kMaxStringLen));
            for (long i = 0; i < times && text.size() < kMaxStringLen; ++i) {
                text += s(0);
            }
            out = Value::string(text);
        }
        else {
            err = "chua ho tro";
            return false;
        }
        return true;
    }

    Interpreter &in_;
    const std::string &s_;
    size_t pos_ = 0;
    bool checking_;
    bool skip_;
    std::string error_;
};

// ---------------------------------------------------------------- Interpreter

bool Interpreter::isBuiltin(const std::string &name, size_t argCount)
{
    for (const BuiltinInfo &b : kBuiltins) {
        if (name == b.name) {
            return argCount >= b.minArgs && argCount <= b.maxArgs;
        }
    }
    return false;
}

uint32_t Interpreter::nextRandom()
{
    // xorshift32
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

bool Interpreter::getVar(const std::string &name, Value &out) const
{
    const auto it = vars_.find(name);
    if (it == vars_.end()) {
        return false;
    }
    out = it->second;
    return true;
}

bool Interpreter::run(const std::string &code, std::string &error)
{
    Exec e(*this, code, false);
    if (e.program()) {
        return true;
    }
    error = e.error();
    return false;
}

bool Interpreter::eval(const std::string &expr, Value &out, std::string &error)
{
    Exec e(*this, expr, false);
    if (e.expressionOnly(out)) {
        return true;
    }
    error = e.error();
    return false;
}

bool Interpreter::checkCode(const std::string &code, std::string &error)
{
    Exec e(*this, code, true);
    if (e.program()) {
        return true;
    }
    error = e.error();
    return false;
}

bool Interpreter::checkExpr(const std::string &expr, std::string &error)
{
    Exec e(*this, expr, true);
    Value ignored;
    if (e.expressionOnly(ignored)) {
        return true;
    }
    error = e.error();
    return false;
}

bool Interpreter::templateParts(const std::string &tmpl, bool checking, std::string &out, std::string &error)
{
    out.clear();
    size_t i = 0;
    while (i < tmpl.size()) {
        const char c = tmpl[i];
        if ((c == '{' || c == '}') && i + 1 < tmpl.size() && tmpl[i + 1] == c) {
            out += c;
            i += 2;
            continue;
        }
        if (c != '{') {
            out += c;
            ++i;
            continue;
        }
        // Find the closing brace, skipping quoted strings inside the expression.
        size_t j = i + 1;
        char quote = 0;
        int depth = 1;
        for (; j < tmpl.size(); ++j) {
            const char d = tmpl[j];
            if (quote != 0) {
                if (d == '\\') {
                    ++j;
                } else if (d == quote) {
                    quote = 0;
                }
            } else if (d == '\'' || d == '"') {
                quote = d;
            } else if (d == '{') {
                ++depth;
            } else if (d == '}' && --depth == 0) {
                break;
            }
        }
        if (j >= tmpl.size()) {
            error = "thieu '}' trong \"" + tmpl + "\"";
            return false;
        }
        const std::string expr = tmpl.substr(i + 1, j - i - 1);
        std::string err;
        if (checking) {
            if (!checkExpr(expr, err)) {
                error = "{" + expr + "}: " + err;
                return false;
            }
        } else {
            Value v;
            if (!eval(expr, v, err)) {
                error = "{" + expr + "}: " + err;
                return false;
            }
            out += v.toString();
        }
        i = j + 1;
    }
    return true;
}

bool Interpreter::interpolate(const std::string &tmpl, std::string &out, std::string &error)
{
    return templateParts(tmpl, false, out, error);
}

bool Interpreter::checkTemplate(const std::string &tmpl, std::string &error)
{
    std::string ignored;
    return templateParts(tmpl, true, ignored, error);
}

}  // namespace mini_script
