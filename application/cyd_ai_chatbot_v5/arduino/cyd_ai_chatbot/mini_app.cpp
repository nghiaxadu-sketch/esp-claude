#include "mini_app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "text_utils.h"

namespace mini_app {
namespace {

constexpr size_t kMaxItems = 80;
constexpr size_t kMaxTimers = 8;
constexpr uint32_t kMinTimerMs = 50;
constexpr uint32_t kDefaultTickMs = 1000;
constexpr uint8_t kMaxTextSize = 8;
constexpr size_t kMaxTextLines = 24;
constexpr int16_t kButtonPadding = 4;
constexpr int16_t kDefaultRadius = 6;
constexpr int16_t kBannerH = 30;
constexpr size_t kBannerChars = 52;

constexpr uint16_t rgb565(uint32_t rgb)
{
    return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kDefaultButton = rgb565(0x3A3A40);
constexpr uint16_t kDefaultBarTrack = rgb565(0x303036);
constexpr uint16_t kDefaultBarFill = rgb565(0x4CAF50);
constexpr uint16_t kBannerColor = rgb565(0xB71C1C);

struct NamedColor {
    const char *name;
    uint32_t rgb;
};

constexpr NamedColor kNamedColors[] = {
    {"black", 0x000000},  {"white", 0xFFFFFF},  {"red", 0xF44336},    {"green", 0x4CAF50},
    {"blue", 0x2196F3},   {"yellow", 0xFFEB3B}, {"orange", 0xFF9800}, {"purple", 0x9C27B0},
    {"pink", 0xE91E63},   {"cyan", 0x00BCD4},   {"teal", 0x009688},   {"gray", 0x9E9E9E},
    {"grey", 0x9E9E9E},   {"darkgray", 0x424242}, {"brown", 0x795548}, {"lime", 0xCDDC39},
    {"navy", 0x1A237E},   {"gold", 0xFFC107},   {"magenta", 0xFF00FF}, {"claude", 0xD97757},
};

// Field kinds for validation: n = number/bool expression, t = text template, c = color, s = script.
struct FieldSpec {
    const char *name;
    char kind;
};

constexpr FieldSpec kCommonFields[] = {{"type", 'x'}, {"id", 'x'}, {"note", 'x'}, {"show", 'n'}, {"on_tap", 's'}};
constexpr FieldSpec kLabelFields[] = {{"x", 'n'}, {"y", 'n'}, {"w", 'n'}, {"size", 'n'}, {"align", 'x'},
                                      {"text", 't'}, {"color", 'c'}};
constexpr FieldSpec kButtonFields[] = {{"x", 'n'}, {"y", 'n'}, {"w", 'n'}, {"h", 'n'}, {"size", 'n'},
                                       {"radius", 'n'}, {"text", 't'}, {"color", 'c'}, {"text_color", 'c'}};
constexpr FieldSpec kRectFields[] = {{"x", 'n'}, {"y", 'n'}, {"w", 'n'}, {"h", 'n'}, {"radius", 'n'},
                                     {"fill", 'n'}, {"color", 'c'}};
constexpr FieldSpec kCircleFields[] = {{"x", 'n'}, {"y", 'n'}, {"r", 'n'}, {"fill", 'n'}, {"color", 'c'}};
constexpr FieldSpec kLineFields[] = {{"x", 'n'}, {"y", 'n'}, {"x2", 'n'}, {"y2", 'n'}, {"color", 'c'}};
constexpr FieldSpec kBarFields[] = {{"x", 'n'}, {"y", 'n'}, {"w", 'n'}, {"h", 'n'}, {"value", 'n'},
                                    {"max", 'n'}, {"color", 'c'}, {"bg", 'c'}};

struct KindSpec {
    const char *type;
    const FieldSpec *fields;
    size_t count;
};

template <size_t N>
constexpr KindSpec kind(const char *type, const FieldSpec (&fields)[N])
{
    return {type, fields, N};
}

// Same order as MiniApp::Kind.
const KindSpec kKinds[] = {
    kind("label", kLabelFields), kind("button", kButtonFields), kind("rect", kRectFields),
    kind("circle", kCircleFields), kind("line", kLineFields),   kind("bar", kBarFields),
};

int16_t clampCoord(double v)
{
    return (int16_t)std::lround(std::fmin(std::fmax(v, -2000.0), 2000.0));
}

std::string lower(std::string s)
{
    for (char &c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
    }
    return s;
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

std::string jsonNumber(double n)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.10g", n);
    return buf;
}

}  // namespace

bool parseColor(const std::string &text, uint16_t &out)
{
    std::string s = lower(text);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    for (const NamedColor &c : kNamedColors) {
        if (s == c.name) {
            out = rgb565(c.rgb);
            return true;
        }
    }
    if (!s.empty() && s[0] == '#') {
        s.erase(0, 1);
    } else if (s.size() > 2 && s[0] == '0' && s[1] == 'x') {
        s.erase(0, 2);
    }
    if (s.size() == 3) {
        s = std::string{s[0], s[0], s[1], s[1], s[2], s[2]};
    }
    if (s.size() != 6) {
        return false;
    }
    uint32_t rgb = 0;
    for (char c : s) {
        const int d = hexDigit(c);
        if (d < 0) {
            return false;
        }
        rgb = (rgb << 4) | (uint32_t)d;
    }
    out = rgb565(rgb);
    return true;
}

bool MiniApp::Bounds::intersects(const Bounds &o) const
{
    return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
}

bool MiniApp::Bounds::contains(int16_t px, int16_t py) const
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

bool MiniApp::Resolved::operator==(const Resolved &o) const
{
    return visible == o.visible && box.x == o.box.x && box.y == o.box.y && box.w == o.box.w && box.h == o.box.h &&
           x == o.x && y == o.y && w == o.w && h == o.h && x2 == o.x2 && y2 == o.y2 && radius == o.radius &&
           color == o.color && color2 == o.color2 && size == o.size && fill == o.fill && align == o.align &&
           std::fabs(fraction - o.fraction) < 1e-4 && lines == o.lines;
}

MiniApp::MiniApp(Canvas &canvas, Device &device) : canvas_(canvas), device_(device)
{
    interp_.setHost(this);
}

// ---------------------------------------------------------------- loading and validation

bool MiniApp::load(const std::string &json, std::string &error)
{
    started_ = false;
    exitRequested_ = false;
    runtimeError_.clear();
    items_.clear();
    timers_.clear();
    persist_.clear();
    if (!mini_json::parse(json, doc_, error)) {
        return false;
    }
    if (!doc_.isObject()) {
        error = "App phai la mot object JSON {...}";
        return false;
    }
    name_ = doc_.getString("name", "App");
    if (const mini_json::Value *p = doc_.get("persist")) {
        if (!p->isArray()) {
            error = "\"persist\" phai la mang ten bien";
            return false;
        }
        for (const mini_json::Value &v : p->items) {
            if (v.isString()) {
                persist_.push_back(v.text);
            }
        }
    }
    if (const mini_json::Value *vars = doc_.get("vars")) {
        if (!vars->isObject()) {
            error = "\"vars\" phai la object {\"ten\": gia_tri}";
            return false;
        }
    }
    return parseItems(error) && parseTimers(error) && checkAll(error);
}

bool MiniApp::parseItems(std::string &error)
{
    const mini_json::Value *items = doc_.get("items");
    if (items == nullptr || !items->isArray()) {
        error = "Thieu mang \"items\"";
        return false;
    }
    if (items->items.size() > kMaxItems) {
        error = "Qua nhieu items (toi da " + std::to_string(kMaxItems) + ")";
        return false;
    }
    for (size_t i = 0; i < items->items.size(); ++i) {
        const mini_json::Value &spec = items->items[i];
        const std::string type = spec.getString("type");
        size_t k = 0;
        while (k < sizeof(kKinds) / sizeof(kKinds[0]) && type != kKinds[k].type) {
            ++k;
        }
        if (!spec.isObject() || k == sizeof(kKinds) / sizeof(kKinds[0])) {
            error = "items[" + std::to_string(i) + "]: type \"" + type +
                    "\" khong ho tro (chi co label, button, rect, circle, line, bar)";
            return false;
        }
        items_.push_back({(Kind)k, &spec, Resolved()});
    }
    return true;
}

bool MiniApp::parseTimers(std::string &error)
{
    if (const mini_json::Value *code = doc_.get("on_tick")) {
        if (!code->isString()) {
            error = "\"on_tick\" phai la chuoi script";
            return false;
        }
        const double ms = doc_.getNumber("tick", kDefaultTickMs);
        timers_.push_back({code->text, (uint32_t)std::max<double>(ms, kMinTimerMs), 0});
    }
    if (const mini_json::Value *list = doc_.get("timers")) {
        if (!list->isArray()) {
            error = "\"timers\" phai la mang [{\"ms\":1000,\"do\":\"...\"}]";
            return false;
        }
        for (const mini_json::Value &t : list->items) {
            const mini_json::Value *code = t.get("do");
            if (code == nullptr || !code->isString()) {
                error = "Moi timer can \"ms\" va \"do\" (chuoi script)";
                return false;
            }
            const double ms = t.getNumber("ms", kDefaultTickMs);
            timers_.push_back({code->text, (uint32_t)std::max<double>(ms, kMinTimerMs), 0});
        }
    }
    if (timers_.size() > kMaxTimers) {
        error = "Toi da " + std::to_string(kMaxTimers) + " timers";
        return false;
    }
    return true;
}

void MiniApp::declareVars()
{
    interp_.clearVars();
    interp_.setVar("tx", mini_script::Value::number(0));
    interp_.setVar("ty", mini_script::Value::number(0));
    const mini_json::Value *vars = doc_.get("vars");
    if (vars == nullptr) {
        return;
    }
    for (size_t i = 0; i < vars->keys.size(); ++i) {
        const mini_json::Value &v = vars->items[i];
        if (v.isString()) {
            interp_.setVar(vars->keys[i], mini_script::Value::string(v.text));
        } else if (v.isBool()) {
            interp_.setVar(vars->keys[i], mini_script::Value::number(v.boolean ? 1 : 0));
        } else {
            interp_.setVar(vars->keys[i], mini_script::Value::number(v.number));
        }
    }
}

bool MiniApp::checkField(const mini_json::Value &spec, const char *field, char kind, std::string &error)
{
    const mini_json::Value *v = spec.get(field);
    if (v == nullptr || kind == 'x') {
        return true;
    }
    std::string err;
    bool ok = true;
    switch (kind) {
    case 'n':
        ok = v->isNumber() || v->isBool() || (v->isString() && interp_.checkExpr(v->text, err));
        if (!v->isNumber() && !v->isBool() && !v->isString()) {
            err = "phai la so hoac bieu thuc";
        }
        break;
    case 't':
        ok = v->isNumber() || (v->isString() && interp_.checkTemplate(v->text, err));
        break;
    case 'c':
        if (v->isNumber()) {
            break;
        }
        if (!v->isString()) {
            ok = false;
            err = "mau phai la chuoi \"#RRGGBB\"";
        } else if (v->text.find('{') != std::string::npos) {
            ok = interp_.checkTemplate(v->text, err);
        } else {
            uint16_t c;
            ok = parseColor(v->text, c);
            if (!ok) {
                err = "mau \"" + v->text + "\" khong hop le";
            }
        }
        break;
    case 's':
        ok = v->isString() && interp_.checkCode(v->text, err);
        if (!v->isString()) {
            err = "phai la chuoi script";
        }
        break;
    default:
        break;
    }
    if (!ok) {
        error = std::string("\"") + field + "\": " + err;
    }
    return ok;
}

bool MiniApp::checkItem(const Item &item, std::string &error)
{
    const KindSpec &ks = kKinds[(size_t)item.kind];
    const mini_json::Value &spec = *item.spec;
    for (const std::string &key : spec.keys) {
        bool known = false;
        for (const FieldSpec &f : kCommonFields) {
            known = known || key == f.name;
        }
        for (size_t i = 0; i < ks.count; ++i) {
            known = known || key == ks.fields[i].name;
        }
        if (!known) {
            error = "truong \"" + key + "\" khong dung cho " + ks.type;
            return false;
        }
    }
    for (const FieldSpec &f : kCommonFields) {
        if (!checkField(spec, f.name, f.kind, error)) {
            return false;
        }
    }
    for (size_t i = 0; i < ks.count; ++i) {
        if (!checkField(spec, ks.fields[i].name, ks.fields[i].kind, error)) {
            return false;
        }
    }
    return true;
}

bool MiniApp::checkAll(std::string &error)
{
    declareVars();
    for (const char *field : {"init", "on_touch", "on_tick"}) {
        if (!checkField(doc_, field, 's', error)) {
            return false;
        }
    }
    if (!checkField(doc_, "bg", 'c', error) || !checkField(doc_, "color", 'c', error)) {
        return false;
    }
    for (size_t i = 0; i < timers_.size(); ++i) {
        std::string err;
        if (!interp_.checkCode(timers_[i].code, err)) {
            error = "timers[" + std::to_string(i) + "]: " + err;
            return false;
        }
    }
    for (size_t i = 0; i < items_.size(); ++i) {
        std::string err;
        if (!checkItem(items_[i], err)) {
            error = "items[" + std::to_string(i) + "] (" + kKinds[(size_t)items_[i].kind].type + "): " + err;
            return false;
        }
    }
    for (const std::string &name : persist_) {
        if (!interp_.hasVar(name)) {
            error = "\"persist\": bien '" + name + "' chua khai bao trong \"vars\"";
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------- running

bool MiniApp::start(std::string &error)
{
    exitRequested_ = false;
    runtimeError_.clear();
    declareVars();

    mini_json::Value saved;
    std::string ignored;
    if (!savedState_.empty() && mini_json::parse(savedState_, saved, ignored) && saved.isObject()) {
        for (const std::string &name : persist_) {
            const mini_json::Value *v = saved.get(name);
            if (v != nullptr && v->isString()) {
                interp_.setVar(name, mini_script::Value::string(v->text));
            } else if (v != nullptr && v->isNumber()) {
                interp_.setVar(name, mini_script::Value::number(v->number));
            }
        }
    }

    const uint32_t now = device_.millis();
    for (Timer &t : timers_) {
        t.nextMs = now + t.intervalMs;
    }
    for (Item &item : items_) {
        item.last = Resolved();
    }
    started_ = true;
    needsFullRedraw_ = true;
    if (const mini_json::Value *init = doc_.get("init")) {
        runCode(init->text, "init");
    }
    render(true);
    if (hasError()) {
        error = runtimeError_;
        return false;
    }
    return true;
}

void MiniApp::fail(const std::string &error)
{
    if (runtimeError_.empty()) {
        runtimeError_ = error;
        drawErrorBanner();
    }
}

bool MiniApp::runCode(const std::string &code, const char *where)
{
    if (hasError()) {
        return false;
    }
    std::string err;
    if (!interp_.run(code, err)) {
        fail(std::string(where) + ": " + err);
        return false;
    }
    return true;
}

void MiniApp::loop()
{
    if (!started_ || hasError() || exitRequested_) {
        return;
    }
    const uint32_t now = device_.millis();
    bool ran = false;
    for (size_t i = 0; i < timers_.size() && !hasError(); ++i) {
        Timer &t = timers_[i];
        if ((int32_t)(now - t.nextMs) < 0) {
            continue;
        }
        t.nextMs += t.intervalMs;
        if ((int32_t)(now - t.nextMs) >= 0) {
            t.nextMs = now + t.intervalMs;  // fell behind (slow HTTP call...), skip missed ticks
        }
        runCode(t.code, "timer");
        ran = true;
    }
    if (ran && !hasError()) {
        render(needsFullRedraw_);
    }
}

void MiniApp::touch(int16_t x, int16_t y)
{
    if (!started_ || hasError() || exitRequested_) {
        return;
    }
    interp_.setVar("tx", mini_script::Value::number(x));
    interp_.setVar("ty", mini_script::Value::number(y));
    bool handled = false;
    for (size_t i = items_.size(); i-- > 0;) {
        Item &item = items_[i];
        const mini_json::Value *tap = item.spec->get("on_tap");
        if (tap == nullptr || !item.last.visible || !item.last.box.contains(x, y)) {
            continue;
        }
        if (item.kind == Kind::Button) {
            const Resolved &r = item.last;
            canvas_.drawRoundRect(r.x, r.y, r.w, r.h, r.radius, kWhite);
            canvas_.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, r.radius, kWhite);
            item.last.visible = false;  // forces a clean redraw after the press highlight
        }
        runCode(tap->text, "on_tap");
        handled = true;
        break;
    }
    if (!handled) {
        if (const mini_json::Value *code = doc_.get("on_touch")) {
            runCode(code->text, "on_touch");
        }
    }
    if (!hasError()) {
        render(needsFullRedraw_);
    }
}

std::vector<std::pair<int16_t, int16_t>> MiniApp::tapTargets() const
{
    std::vector<std::pair<int16_t, int16_t>> targets;
    for (const Item &item : items_) {
        if (item.spec->get("on_tap") != nullptr && item.last.visible) {
            targets.emplace_back(item.last.box.x + item.last.box.w / 2, item.last.box.y + item.last.box.h / 2);
        }
    }
    return targets;
}

void MiniApp::redraw()
{
    needsFullRedraw_ = true;
    if (started_) {
        render(true);
        if (hasError()) {
            drawErrorBanner();
        }
    }
}

std::string MiniApp::saveState() const
{
    if (persist_.empty()) {
        return std::string();
    }
    std::string out = "{";
    for (const std::string &name : persist_) {
        mini_script::Value v;
        if (!interp_.getVar(name, v)) {
            continue;
        }
        if (out.size() > 1) {
            out += ",";
        }
        out += mini_json::quote(name) + ":" + (v.isText ? mini_json::quote(v.text) : jsonNumber(v.num));
    }
    return out + "}";
}

bool MiniApp::hasFunction(const std::string &name, size_t argCount) const
{
    if (name == "exit" || name == "save") {
        return argCount == 0;
    }
    return device_.hasFunction(name, argCount);
}

bool MiniApp::callFunction(const std::string &name, const std::vector<mini_script::Value> &args,
                           mini_script::Value &result, std::string &error)
{
    result = mini_script::Value::number(0);
    if (name == "exit") {
        exitRequested_ = true;
        return true;
    }
    if (name == "save") {
        saveRequested_ = true;
        return true;
    }
    if (rendering_ && name == "http") {
        error = "chi goi http() trong init/timers/on_tap, luu ket qua vao bien roi hien bien do";
        return false;
    }
    return device_.callFunction(name, args, result, error);
}

// ---------------------------------------------------------------- field evaluation

bool MiniApp::numberField(const mini_json::Value &spec, const char *field, double fallback, double &out,
                          std::string &error)
{
    const mini_json::Value *v = spec.get(field);
    if (v == nullptr) {
        out = fallback;
        return true;
    }
    if (v->isNumber()) {
        out = v->number;
        return true;
    }
    if (v->isBool()) {
        out = v->boolean ? 1 : 0;
        return true;
    }
    mini_script::Value result;
    std::string err;
    if (!v->isString() || !interp_.eval(v->text, result, err)) {
        error = std::string("\"") + field + "\": " + err;
        return false;
    }
    out = result.toNumber();
    return true;
}

bool MiniApp::boolField(const mini_json::Value &spec, const char *field, bool fallback, bool &out,
                        std::string &error)
{
    const mini_json::Value *v = spec.get(field);
    if (v == nullptr) {
        out = fallback;
        return true;
    }
    if (v->isString()) {
        mini_script::Value result;
        std::string err;
        if (!interp_.eval(v->text, result, err)) {
            error = std::string("\"") + field + "\": " + err;
            return false;
        }
        out = result.truthy();
        return true;
    }
    out = v->isBool() ? v->boolean : std::fabs(v->number) > 1e-9;
    return true;
}

bool MiniApp::textField(const mini_json::Value &spec, const char *field, std::string &out, std::string &error)
{
    const mini_json::Value *v = spec.get(field);
    out.clear();
    if (v == nullptr) {
        return true;
    }
    if (v->isNumber()) {
        out = mini_script::formatNumber(v->number);
        return true;
    }
    std::string err;
    if (v->isString() && !interp_.interpolate(v->text, out, err)) {
        error = std::string("\"") + field + "\": " + err;
        return false;
    }
    return true;
}

bool MiniApp::colorField(const mini_json::Value &spec, const char *field, uint16_t fallback, uint16_t &out,
                         std::string &error)
{
    const mini_json::Value *v = spec.get(field);
    out = fallback;
    if (v == nullptr) {
        return true;
    }
    if (v->isNumber()) {
        out = rgb565((uint32_t)v->number);
        return true;
    }
    std::string text;
    if (!textField(spec, field, text, error)) {
        return false;
    }
    if (!parseColor(text, out)) {
        error = std::string("\"") + field + "\": mau \"" + text + "\" khong hop le";
        return false;
    }
    return true;
}

void MiniApp::layoutText(const std::string &utf8, Resolved &r, int16_t boxW)
{
    const std::string ascii = text_utils::utf8ToAscii(utf8);
    const int16_t charW = kFontW * r.size;
    if (boxW > 0) {
        r.lines = text_utils::wrapText(ascii, (size_t)std::max<int16_t>(1, boxW / charW));
    } else {
        r.lines = text_utils::wrapText(ascii, (size_t)std::max<int16_t>(1, kScreenW / charW));
    }
    if (r.lines.size() > kMaxTextLines) {
        r.lines.resize(kMaxTextLines);
    }
}

bool MiniApp::resolve(const Item &item, Resolved &r, std::string &error)
{
    const mini_json::Value &spec = *item.spec;
    r = Resolved();
    bool show = true;
    if (!boolField(spec, "show", true, show, error)) {
        return false;
    }
    if (!show) {
        return true;
    }
    r.visible = true;
    double x = 0, y = 0, w = 0, h = 0, size = 2, radius = 0;
    if (!numberField(spec, "x", 0, x, error) || !numberField(spec, "y", 0, y, error)) {
        return false;
    }
    r.x = clampCoord(x);
    r.y = clampCoord(y);

    switch (item.kind) {
    case Kind::Label: {
        std::string text;
        if (!numberField(spec, "w", 0, w, error) || !numberField(spec, "size", 2, size, error) ||
            !textField(spec, "text", text, error) || !colorField(spec, "color", kWhite, r.color, error)) {
            return false;
        }
        r.size = (uint8_t)std::min<double>(std::max<double>(std::lround(size), 1), kMaxTextSize);
        r.w = clampCoord(w);
        const std::string align = spec.getString("align", "left");
        r.align = align == "center" ? Align::Center : align == "right" ? Align::Right : Align::Left;
        layoutText(text, r, r.w);
        size_t longest = 0;
        for (const std::string &line : r.lines) {
            longest = std::max(longest, line.size());
        }
        const int16_t lineH = kFontH * r.size + r.size;
        r.box = {r.x, r.y, r.w > 0 ? r.w : (int16_t)(longest * kFontW * r.size), (int16_t)(r.lines.size() * lineH)};
        return true;
    }
    case Kind::Button: {
        std::string text;
        if (!numberField(spec, "w", 90, w, error) || !numberField(spec, "h", 40, h, error) ||
            !numberField(spec, "size", 2, size, error) || !numberField(spec, "radius", kDefaultRadius, radius, error) ||
            !textField(spec, "text", text, error) || !colorField(spec, "color", kDefaultButton, r.color, error) ||
            !colorField(spec, "text_color", kWhite, r.color2, error)) {
            return false;
        }
        r.w = clampCoord(w);
        r.h = clampCoord(h);
        r.radius = clampCoord(std::min(radius, std::min(w, h) / 2));
        r.size = (uint8_t)std::min<double>(std::max<double>(std::lround(size), 1), kMaxTextSize);
        layoutText(text, r, r.w - 2 * kButtonPadding);
        r.box = {r.x, r.y, r.w, r.h};
        return true;
    }
    case Kind::Rect: {
        if (!numberField(spec, "w", 10, w, error) || !numberField(spec, "h", 10, h, error) ||
            !numberField(spec, "radius", 0, radius, error) || !boolField(spec, "fill", true, r.fill, error) ||
            !colorField(spec, "color", kWhite, r.color, error)) {
            return false;
        }
        r.w = clampCoord(w);
        r.h = clampCoord(h);
        r.radius = clampCoord(std::min(radius, std::min(w, h) / 2));
        r.box = {r.x, r.y, r.w, r.h};
        return true;
    }
    case Kind::Circle: {
        if (!numberField(spec, "r", 10, radius, error) || !boolField(spec, "fill", true, r.fill, error) ||
            !colorField(spec, "color", kWhite, r.color, error)) {
            return false;
        }
        r.radius = clampCoord(std::fabs(radius));
        r.box = {(int16_t)(r.x - r.radius), (int16_t)(r.y - r.radius), (int16_t)(2 * r.radius + 1),
                 (int16_t)(2 * r.radius + 1)};
        return true;
    }
    case Kind::Line: {
        double x2 = 0, y2 = 0;
        if (!numberField(spec, "x2", x, x2, error) || !numberField(spec, "y2", y, y2, error) ||
            !colorField(spec, "color", kWhite, r.color, error)) {
            return false;
        }
        r.x2 = clampCoord(x2);
        r.y2 = clampCoord(y2);
        const int16_t left = std::min(r.x, r.x2), top = std::min(r.y, r.y2);
        r.box = {left, top, (int16_t)(std::max(r.x, r.x2) - left + 1), (int16_t)(std::max(r.y, r.y2) - top + 1)};
        return true;
    }
    case Kind::Bar: {
        double value = 0, max = 100;
        if (!numberField(spec, "w", 100, w, error) || !numberField(spec, "h", 12, h, error) ||
            !numberField(spec, "value", 0, value, error) || !numberField(spec, "max", 100, max, error) ||
            !colorField(spec, "color", kDefaultBarFill, r.color, error) ||
            !colorField(spec, "bg", kDefaultBarTrack, r.color2, error)) {
            return false;
        }
        r.w = clampCoord(w);
        r.h = clampCoord(h);
        r.fraction = max > 0 ? std::fmin(std::fmax(value / max, 0), 1) : 0;
        r.box = {r.x, r.y, r.w, r.h};
        return true;
    }
    }
    return true;
}

// ---------------------------------------------------------------- drawing

void MiniApp::drawItem(const Item &item, const Resolved &r)
{
    switch (item.kind) {
    case Kind::Label: {
        const int16_t lineH = kFontH * r.size + r.size;
        for (size_t i = 0; i < r.lines.size(); ++i) {
            const int16_t lineW = (int16_t)(r.lines[i].size() * kFontW * r.size);
            int16_t lx = r.x;
            if (r.w > 0 && r.align == Align::Center) {
                lx = r.x + (r.w - lineW) / 2;
            } else if (r.w > 0 && r.align == Align::Right) {
                lx = r.x + r.w - lineW;
            }
            canvas_.drawText(lx, (int16_t)(r.y + i * lineH), r.lines[i], r.size, r.color);
        }
        break;
    }
    case Kind::Button: {
        canvas_.fillRoundRect(r.x, r.y, r.w, r.h, r.radius, r.color);
        const int16_t lineH = kFontH * r.size + r.size;
        const int16_t textH = (int16_t)(r.lines.size() * lineH) - r.size;
        int16_t ly = r.y + (r.h - textH) / 2;
        for (const std::string &line : r.lines) {
            const int16_t lineW = (int16_t)(line.size() * kFontW * r.size);
            canvas_.drawText(r.x + (r.w - lineW) / 2, ly, line, r.size, r.color2);
            ly += lineH;
        }
        break;
    }
    case Kind::Rect:
        if (r.fill) {
            r.radius > 0 ? canvas_.fillRoundRect(r.x, r.y, r.w, r.h, r.radius, r.color)
                         : canvas_.fillRect(r.x, r.y, r.w, r.h, r.color);
        } else {
            r.radius > 0 ? canvas_.drawRoundRect(r.x, r.y, r.w, r.h, r.radius, r.color)
                         : canvas_.drawRect(r.x, r.y, r.w, r.h, r.color);
        }
        break;
    case Kind::Circle:
        r.fill ? canvas_.fillCircle(r.x, r.y, r.radius, r.color) : canvas_.drawCircle(r.x, r.y, r.radius, r.color);
        break;
    case Kind::Line:
        canvas_.drawLine(r.x, r.y, r.x2, r.y2, r.color);
        break;
    case Kind::Bar: {
        canvas_.fillRect(r.x, r.y, r.w, r.h, r.color2);
        const int16_t filled = (int16_t)std::lround(r.w * r.fraction);
        if (filled > 0) {
            canvas_.fillRect(r.x, r.y, filled, r.h, r.color);
        }
        break;
    }
    }
}

void MiniApp::render(bool full)
{
    std::string err;
    uint16_t bg = kBlack;
    rendering_ = true;
    if (!colorField(doc_, "bg", kBlack, bg, err)) {
        rendering_ = false;
        fail("bg: " + err);
        return;
    }
    std::vector<Resolved> next(items_.size());
    for (size_t i = 0; i < items_.size(); ++i) {
        if (!resolve(items_[i], next[i], err)) {
            rendering_ = false;
            fail("items[" + std::to_string(i) + "] (" + kKinds[(size_t)items_[i].kind].type + "): " + err);
            return;
        }
    }
    rendering_ = false;

    if (bg != bg_) {
        full = true;
        bg_ = bg;
    }
    if (full) {
        canvas_.fillRect(0, 0, kScreenW, kScreenH, bg_);
        for (size_t i = 0; i < items_.size(); ++i) {
            if (next[i].visible) {
                drawItem(items_[i], next[i]);
            }
            items_[i].last = next[i];
        }
        needsFullRedraw_ = false;
        return;
    }

    // Partial redraw: clear where changed items were, then repaint everything touching changed areas.
    std::vector<Bounds> regions;
    std::vector<bool> changed(items_.size(), false);
    for (size_t i = 0; i < items_.size(); ++i) {
        if (next[i] == items_[i].last) {
            continue;
        }
        changed[i] = true;
        if (items_[i].last.visible) {
            canvas_.fillRect(items_[i].last.box.x, items_[i].last.box.y, items_[i].last.box.w, items_[i].last.box.h,
                             bg_);
            regions.push_back(items_[i].last.box);
        }
        if (next[i].visible) {
            regions.push_back(next[i].box);
        }
    }
    for (size_t i = 0; i < items_.size(); ++i) {
        if (!next[i].visible) {
            continue;
        }
        bool draw = changed[i];
        for (size_t k = 0; !draw && k < regions.size(); ++k) {
            draw = next[i].box.intersects(regions[k]);
        }
        if (draw) {
            drawItem(items_[i], next[i]);
        }
    }
    for (size_t i = 0; i < items_.size(); ++i) {
        items_[i].last = next[i];
    }
}

void MiniApp::drawErrorBanner()
{
    canvas_.fillRect(0, kScreenH - kBannerH, kScreenW, kBannerH, kBannerColor);
    const std::string ascii = text_utils::utf8ToAscii("Loi: " + runtimeError_);
    canvas_.drawText(2, kScreenH - kBannerH + 3, ascii.substr(0, kBannerChars), 1, kWhite);
    canvas_.drawText(2, kScreenH - kBannerH + 13,
                     ascii.size() > kBannerChars ? ascii.substr(kBannerChars, kBannerChars) : std::string(), 1, kWhite);
    canvas_.drawText(2, kScreenH - 8, "Giu man hinh 1s de thoat, chon Sua de Claude sua", 1, kWhite);
}

}  // namespace mini_app
