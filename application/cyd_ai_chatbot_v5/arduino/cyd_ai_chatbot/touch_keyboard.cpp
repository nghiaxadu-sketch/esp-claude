#include "touch_keyboard.h"

namespace {

using namespace ui;

constexpr int16_t kInputTop = kHeaderH + 4;
constexpr int16_t kInputH = 36;
constexpr int16_t kInputTextSize = 2;
constexpr int16_t kKeysTop = kInputTop + kInputH + 2;
constexpr int16_t kKeyRowH = (kScreenH - kKeysTop) / 4;
constexpr int16_t kKeyW = kScreenW / 10;
constexpr int16_t kWideKeyW = 48;
constexpr int16_t kSpaceW = 128;
constexpr int16_t kDoneW = kScreenW - kWideKeyW - kKeyW - kSpaceW - kKeyW;
constexpr int16_t kCancelW = 72;
constexpr uint32_t kKeyFlashMs = 60;

}  // namespace

TouchKeyboard::TouchKeyboard(Adafruit_GFX &gfx) : gfx_(gfx)
{
    cancelButton_ = {kScreenW - kCancelW, 0, kCancelW, kHeaderH};
}

void TouchKeyboard::show(const char *title, const String &initial, const char *placeholder, size_t maxChars,
                         const char *doneLabel)
{
    title_ = title;
    input_ = initial;
    placeholder_ = placeholder;
    maxChars_ = maxChars;
    doneLabel_ = doneLabel;
    mode_ = Mode::Letters;
    shift_ = false;
    buildKeys();
    gfx_.fillScreen(kColorBg);
    drawBackHeader(gfx_, ellipsize(title_, 18).c_str());
    drawButton(gfx_, cancelButton_, "Huy", kColorKeySpecial, kColorWhite);
    drawInputBox();
    drawKeys();
}

void TouchKeyboard::addKey(int16_t x, int16_t y, int16_t w, int16_t h, KeyCode code, char ch, const char *label)
{
    if (keyCount_ < kMaxKeys) {
        keys_[keyCount_++] = {{x, y, w, h}, code, ch, label};
    }
}

void TouchKeyboard::addKeyRow(const char *chars, int16_t x, int16_t y, int16_t keyW, int16_t keyH)
{
    for (const char *c = chars; *c != '\0'; ++c, x += keyW) {
        addKey(x, y, keyW, keyH, KeyCode::Char, *c, nullptr);
    }
}

void TouchKeyboard::buildKeys()
{
    keyCount_ = 0;
    const bool letters = (mode_ == Mode::Letters);
    const int16_t y0 = kKeysTop;
    const int16_t y1 = y0 + kKeyRowH;
    const int16_t y2 = y1 + kKeyRowH;
    const int16_t y3 = y2 + kKeyRowH;

    addKeyRow(letters ? "qwertyuiop" : "1234567890", 0, y0, kKeyW, kKeyRowH);
    addKeyRow(letters ? "asdfghjkl?" : "-/:;()\"'!@", 0, y1, kKeyW, kKeyRowH);

    if (letters) {
        addKey(0, y2, kWideKeyW, kKeyRowH, KeyCode::Shift, 0, "Aa");
    } else {
        addKey(0, y2, kWideKeyW, kKeyRowH, KeyCode::Char, '#', nullptr);
    }
    addKeyRow(letters ? "zxcvbnm" : "%&*+=_$", kWideKeyW, y2, (kScreenW - 2 * kWideKeyW) / 7, kKeyRowH);
    addKey(kScreenW - kWideKeyW, y2, kWideKeyW, kKeyRowH, KeyCode::Delete, 0, "<-");

    int16_t x = 0;
    addKey(x, y3, kWideKeyW, kKeyRowH, KeyCode::Mode, 0, letters ? "123" : "abc");
    x += kWideKeyW;
    addKey(x, y3, kKeyW, kKeyRowH, KeyCode::Char, ',', nullptr);
    x += kKeyW;
    addKey(x, y3, kSpaceW, kKeyRowH, KeyCode::Space, ' ', "space");
    x += kSpaceW;
    addKey(x, y3, kKeyW, kKeyRowH, KeyCode::Char, '.', nullptr);
    x += kKeyW;
    addKey(x, y3, kDoneW, kKeyRowH, KeyCode::Done, 0, nullptr);
}

void TouchKeyboard::drawKey(const Key &key, bool pressed)
{
    const Rect inner = {int16_t(key.r.x + 2), int16_t(key.r.y + 2), int16_t(key.r.w - 4), int16_t(key.r.h - 4)};
    uint16_t bg = key.code == KeyCode::Char ? kColorKey : kColorKeySpecial;
    if (key.code == KeyCode::Done) {
        bg = kColorAccent;
    }
    if (key.code == KeyCode::Shift && shift_) {
        bg = kColorUser;
    }
    if (pressed) {
        bg = kColorWhite;
    }
    const uint16_t fg = pressed ? kColorBg : kColorWhite;
    const char *label = key.code == KeyCode::Done ? doneLabel_ : key.label;
    if (label != nullptr) {
        drawButton(gfx_, inner, label, bg, fg, strlen(label) > 3 ? 1 : 2);
        return;
    }
    const char ch = (shift_ && mode_ == Mode::Letters) ? (char)toupper(key.ch) : key.ch;
    const char text[2] = {ch, '\0'};
    drawButton(gfx_, inner, text, bg, fg);
}

void TouchKeyboard::drawKeys()
{
    gfx_.fillRect(0, kKeysTop, kScreenW, kScreenH - kKeysTop, kColorBg);
    for (size_t i = 0; i < keyCount_; ++i) {
        drawKey(keys_[i], false);
    }
}

void TouchKeyboard::drawInputBox()
{
    const Rect box = {kMargin, kInputTop, kScreenW - 2 * kMargin, kInputH};
    gfx_.fillRoundRect(box.x, box.y, box.w, box.h, 4, kColorHeader);
    gfx_.setTextSize(kInputTextSize);
    const int16_t charW = kFontW * kInputTextSize;
    const size_t perLine = (box.w - 8) / charW;
    if (input_.length() == 0) {
        gfx_.setTextColor(kColorMuted);
        gfx_.setCursor(box.x + 4, box.y + 2);
        gfx_.print(placeholder_);
        return;
    }
    // Show the last two lines of the input, wrapped by character, with a cursor.
    String shown = input_ + "_";
    const size_t maxShown = perLine * 2;
    if (shown.length() > maxShown) {
        // Drop whole lines from the front so the visible lines keep the same breaks while typing.
        const size_t overflow = shown.length() - maxShown;
        shown = shown.substring((overflow + perLine - 1) / perLine * perLine);
    }
    gfx_.setTextColor(kColorText);
    for (int line = 0; line < 2 && line * perLine < shown.length(); ++line) {
        gfx_.setCursor(box.x + 4, box.y + 2 + line * (kFontH * kInputTextSize + 2));
        gfx_.print(shown.substring(line * perLine, min(shown.length(), (line + 1) * perLine)));
    }
}

KeyboardResult TouchKeyboard::handleTouch(int16_t x, int16_t y)
{
    if (cancelButton_.contains(x, y) || backTapped(x, y)) {
        return KeyboardResult::Cancel;
    }
    for (size_t i = 0; i < keyCount_; ++i) {
        const Key &key = keys_[i];
        if (!key.r.contains(x, y)) {
            continue;
        }
        drawKey(key, true);
        delay(kKeyFlashMs);
        drawKey(key, false);

        switch (key.code) {
        case KeyCode::Char:
        case KeyCode::Space:
            if (input_.length() < maxChars_) {
                const bool upper = shift_ && mode_ == Mode::Letters;
                input_ += upper ? (char)toupper(key.ch) : key.ch;
            }
            if (shift_) {
                shift_ = false;
                drawKeys();
            }
            break;
        case KeyCode::Shift:
            shift_ = !shift_;
            drawKeys();
            break;
        case KeyCode::Delete:
            if (input_.length() > 0) {
                input_.remove(input_.length() - 1);
            }
            break;
        case KeyCode::Mode:
            mode_ = (mode_ == Mode::Letters) ? Mode::Symbols : Mode::Letters;
            shift_ = false;
            buildKeys();
            drawKeys();
            break;
        case KeyCode::Done:
            return KeyboardResult::Done;
        }
        drawInputBox();
        return KeyboardResult::None;
    }
    return KeyboardResult::None;
}
