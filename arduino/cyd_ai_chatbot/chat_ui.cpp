#include "chat_ui.h"

#include <string>

#include "config.h"
#include "text_utils.h"

namespace {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t kColorBg = rgb565(0x16, 0x16, 0x18);
constexpr uint16_t kColorHeader = rgb565(0x2A, 0x2A, 0x2E);
constexpr uint16_t kColorAccent = rgb565(0xD9, 0x77, 0x57);  // Claude orange
constexpr uint16_t kColorUser = rgb565(0x7F, 0xD1, 0xFF);
constexpr uint16_t kColorClaude = rgb565(0xF0, 0xED, 0xE6);
constexpr uint16_t kColorSystem = rgb565(0xFF, 0xC8, 0x57);
constexpr uint16_t kColorMuted = rgb565(0x8A, 0x8A, 0x90);
constexpr uint16_t kColorButton = rgb565(0x3A, 0x3A, 0x40);
constexpr uint16_t kColorKey = rgb565(0x33, 0x33, 0x38);
constexpr uint16_t kColorKeySpecial = rgb565(0x4A, 0x4A, 0x52);
constexpr uint16_t kColorWhite = 0xFFFF;

constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr int16_t kHeaderH = 22;
constexpr int16_t kFooterH = 36;
constexpr int16_t kMargin = 4;
constexpr int16_t kScrollbarW = 4;
constexpr int16_t kBodyTop = kHeaderH + 2;
constexpr int16_t kBodyBottom = kScreenH - kFooterH - 2;
constexpr int16_t kFontW = 6;  // built-in 5x7 font cell at text size 1
constexpr int16_t kFontH = 8;
constexpr int16_t kLineGap = 2;

constexpr int16_t kInputTop = kHeaderH + 4;
constexpr int16_t kInputH = 36;
constexpr int16_t kInputTextSize = 2;
constexpr int16_t kKeysTop = kInputTop + kInputH + 2;
constexpr int16_t kKeyRowH = (kScreenH - kKeysTop) / 4;
constexpr int16_t kKeyW = kScreenW / 10;
constexpr int16_t kWideKeyW = 48;
constexpr int16_t kSpaceW = 128;
constexpr int16_t kSendW = kScreenW - kWideKeyW - kKeyW - kSpaceW - kKeyW;
constexpr size_t kMaxInputChars = 300;
constexpr size_t kMaxChatLines = 300;
constexpr uint32_t kKeyFlashMs = 60;

constexpr const char *kTitle = "Claude Chat";

uint16_t colorFor(Speaker who)
{
    switch (who) {
    case Speaker::User:
        return kColorUser;
    case Speaker::Claude:
        return kColorClaude;
    default:
        return kColorSystem;
    }
}

const char *prefixFor(Speaker who)
{
    switch (who) {
    case Speaker::User:
        return "Ban: ";
    case Speaker::Claude:
        return "Claude: ";
    default:
        return "* ";
    }
}

}  // namespace

ChatUi::ChatUi(Adafruit_ST7789 &tft) : tft_(tft)
{
}

void ChatUi::begin()
{
    lineHeight_ = kFontH * CHAT_TEXT_SIZE + kLineGap;
    visibleLines_ = (kBodyBottom - kBodyTop) / lineHeight_;
    charsPerLine_ = (kScreenW - 2 * kMargin - kScrollbarW - 2) / (kFontW * CHAT_TEXT_SIZE);
    for (size_t i = 0; i < kFooterButtons; ++i) {
        const int16_t w = kScreenW / kFooterButtons;
        footer_[i] = {int16_t(i * w + 2), int16_t(kScreenH - kFooterH + 2), int16_t(w - 4), int16_t(kFooterH - 4)};
    }
    cancelButton_ = {kScreenW - 72, 0, 72, kHeaderH};
    tft_.setTextWrap(false);
    buildKeys();
}

// ---------------------------------------------------------------- drawing helpers

void ChatUi::drawSplash(const char *title, const char *line1, const char *line2)
{
    tft_.fillScreen(kColorBg);
    tft_.setTextSize(2);
    tft_.setTextColor(kColorAccent);
    tft_.setCursor((kScreenW - (int16_t)strlen(title) * kFontW * 2) / 2, 80);
    tft_.print(title);
    tft_.setTextSize(1);
    tft_.setTextColor(kColorClaude);
    const char *lines[] = {line1, line2};
    int16_t y = 116;
    for (const char *line : lines) {
        if (line != nullptr) {
            tft_.setCursor(max(0, (kScreenW - (int16_t)strlen(line) * kFontW) / 2), y);
            tft_.print(line);
            y += 16;
        }
    }
}

void ChatUi::drawHeader(const char *title)
{
    tft_.fillRect(0, 0, kScreenW, kHeaderH, kColorHeader);
    tft_.setTextSize(2);
    tft_.setTextColor(kColorAccent);
    tft_.setCursor(6, 3);
    tft_.print(title);

    if (screen_ == Screen::Keyboard) {
        drawButton(cancelButton_, "Huy", kColorKeySpecial, kColorWhite);
        return;
    }
    const String status = busy_ ? String("Dang nghi...") : status_;
    tft_.setTextSize(1);
    tft_.setTextColor(busy_ ? kColorSystem : kColorMuted);
    tft_.setCursor(kScreenW - (int16_t)status.length() * kFontW - 6, 8);
    tft_.print(status);
}

void ChatUi::drawButton(const Rect &r, const char *label, uint16_t bg, uint16_t fg, uint8_t textSize)
{
    tft_.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
    tft_.setTextSize(textSize);
    tft_.setTextColor(fg);
    const int16_t textW = (int16_t)strlen(label) * kFontW * textSize;
    const int16_t textH = kFontH * textSize;
    tft_.setCursor(r.x + (r.w - textW) / 2, r.y + (r.h - textH) / 2 + 1);
    tft_.print(label);
}

// ---------------------------------------------------------------- chat screen

void ChatUi::showChat()
{
    screen_ = Screen::Chat;
    tft_.fillScreen(kColorBg);
    drawHeader(kTitle);
    drawChatBody();
    drawFooter();
}

void ChatUi::drawChatBody()
{
    if (screen_ != Screen::Chat) {
        return;
    }
    tft_.fillRect(0, kBodyTop, kScreenW, kBodyBottom - kBodyTop, kColorBg);
    tft_.setTextSize(CHAT_TEXT_SIZE);
    for (int i = 0; i < visibleLines_; ++i) {
        const int index = scrollTop_ + i;
        if (index >= (int)lines_.size()) {
            break;
        }
        tft_.setTextColor(lines_[index].color);
        tft_.setCursor(kMargin, kBodyTop + i * lineHeight_);
        tft_.print(lines_[index].text);
    }

    // Scrollbar, only when there is more than one page.
    if ((int)lines_.size() > visibleLines_) {
        const int16_t trackH = kBodyBottom - kBodyTop;
        const int16_t thumbH = max<int16_t>(12, trackH * visibleLines_ / (int)lines_.size());
        const int16_t thumbY = kBodyTop + (trackH - thumbH) * scrollTop_ / max(1, maxScroll());
        tft_.fillRect(kScreenW - kScrollbarW - 1, kBodyTop, kScrollbarW, trackH, kColorHeader);
        tft_.fillRect(kScreenW - kScrollbarW - 1, thumbY, kScrollbarW, thumbH, kColorMuted);
    }
}

void ChatUi::drawFooter()
{
    static const char *const kLabels[kFooterButtons] = {"Len", "Xuong", "Nhap", "Xoa"};
    for (size_t i = 0; i < kFooterButtons; ++i) {
        const bool primary = (i == 2);
        drawButton(footer_[i], kLabels[i], primary ? kColorAccent : kColorButton, kColorWhite);
    }
}

void ChatUi::addMessage(Speaker who, const String &utf8Text)
{
    std::string text = text_utils::utf8ToAscii(utf8Text.c_str());
    if (who == Speaker::Claude) {
        text = text_utils::stripMarkdown(text);
    }
    const std::vector<std::string> wrapped = text_utils::wrapText(prefixFor(who) + text, charsPerLine_);
    if (!lines_.empty()) {
        lines_.push_back({String(), kColorBg});  // spacer between messages
    }
    const uint16_t color = colorFor(who);
    for (const std::string &line : wrapped) {
        lines_.push_back({String(line.c_str()), color});
    }
    if (lines_.size() > kMaxChatLines) {
        lines_.erase(lines_.begin(), lines_.begin() + (lines_.size() - kMaxChatLines));
    }
    scrollToBottom();
    drawChatBody();
}

void ChatUi::clearChat()
{
    lines_.clear();
    scrollTop_ = 0;
    drawChatBody();
}

void ChatUi::setStatus(const String &status)
{
    if (status == status_) {
        return;
    }
    status_ = status;
    if (screen_ == Screen::Chat) {
        drawHeader(kTitle);
    }
}

void ChatUi::setBusy(bool busy)
{
    busy_ = busy;
    if (screen_ == Screen::Chat) {
        drawHeader(kTitle);
    }
}

int ChatUi::maxScroll() const
{
    return max(0, (int)lines_.size() - visibleLines_);
}

void ChatUi::scrollBy(int lines)
{
    const int next = constrain(scrollTop_ + lines, 0, maxScroll());
    if (next != scrollTop_) {
        scrollTop_ = next;
        drawChatBody();
    }
}

void ChatUi::scrollToBottom()
{
    scrollTop_ = maxScroll();
}

UiAction ChatUi::handleChatTouch(int16_t x, int16_t y)
{
    const int page = max(1, visibleLines_ - 2);
    if (footer_[0].contains(x, y)) {
        scrollBy(-page);
    } else if (footer_[1].contains(x, y)) {
        scrollBy(page);
    } else if (footer_[2].contains(x, y)) {
        showKeyboard();
    } else if (footer_[3].contains(x, y)) {
        return UiAction::ClearChat;
    } else if (y >= kBodyTop && y < kBodyBottom) {
        // Tap on the upper/lower half of the chat also scrolls.
        scrollBy(y < (kBodyTop + kBodyBottom) / 2 ? -page : page);
    }
    return UiAction::None;
}

// ---------------------------------------------------------------- keyboard screen

void ChatUi::showKeyboard()
{
    screen_ = Screen::Keyboard;
    tft_.fillScreen(kColorBg);
    drawHeader("Nhap cau hoi");
    drawInputBox();
    drawKeys();
}

void ChatUi::clearInput()
{
    input_ = "";
    shift_ = false;
}

void ChatUi::addKey(int16_t x, int16_t y, int16_t w, int16_t h, KeyCode code, char ch, const char *label)
{
    if (keyCount_ < kMaxKeys) {
        keys_[keyCount_++] = {{x, y, w, h}, code, ch, label};
    }
}

void ChatUi::addKeyRow(const char *chars, int16_t x, int16_t y, int16_t keyW, int16_t keyH)
{
    for (const char *c = chars; *c != '\0'; ++c, x += keyW) {
        addKey(x, y, keyW, keyH, KeyCode::Char, *c, nullptr);
    }
}

void ChatUi::buildKeys()
{
    keyCount_ = 0;
    const bool letters = (mode_ == KeyboardMode::Letters);
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
    addKey(x, y3, kSendW, kKeyRowH, KeyCode::Send, 0, "Gui");
}

void ChatUi::drawKey(const Key &key, bool pressed)
{
    const Rect inner = {int16_t(key.r.x + 2), int16_t(key.r.y + 2), int16_t(key.r.w - 4), int16_t(key.r.h - 4)};
    uint16_t bg = key.code == KeyCode::Char ? kColorKey : kColorKeySpecial;
    if (key.code == KeyCode::Send) {
        bg = kColorAccent;
    }
    if (key.code == KeyCode::Shift && shift_) {
        bg = kColorUser;
    }
    if (pressed) {
        bg = kColorWhite;
    }
    const uint16_t fg = pressed ? kColorBg : kColorWhite;
    if (key.label != nullptr) {
        drawButton(inner, key.label, bg, fg, strlen(key.label) > 3 ? 1 : 2);
        return;
    }
    const char ch = (shift_ && mode_ == KeyboardMode::Letters) ? (char)toupper(key.ch) : key.ch;
    const char label[2] = {ch, '\0'};
    drawButton(inner, label, bg, fg);
}

void ChatUi::drawKeys()
{
    tft_.fillRect(0, kKeysTop, kScreenW, kScreenH - kKeysTop, kColorBg);
    for (size_t i = 0; i < keyCount_; ++i) {
        drawKey(keys_[i], false);
    }
}

void ChatUi::drawInputBox()
{
    const Rect box = {kMargin, kInputTop, kScreenW - 2 * kMargin, kInputH};
    tft_.fillRoundRect(box.x, box.y, box.w, box.h, 4, kColorHeader);
    tft_.setTextSize(kInputTextSize);
    const int16_t charW = kFontW * kInputTextSize;
    const size_t perLine = (box.w - 8) / charW;
    if (input_.length() == 0) {
        tft_.setTextColor(kColorMuted);
        tft_.setCursor(box.x + 4, box.y + 2);
        tft_.print("Go cau hoi...");
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
    tft_.setTextColor(kColorClaude);
    for (int line = 0; line < 2 && line * perLine < shown.length(); ++line) {
        tft_.setCursor(box.x + 4, box.y + 2 + line * (kFontH * kInputTextSize + 2));
        tft_.print(shown.substring(line * perLine, min(shown.length(), (line + 1) * perLine)));
    }
}

UiAction ChatUi::handleKeyboardTouch(int16_t x, int16_t y)
{
    if (cancelButton_.contains(x, y)) {
        showChat();
        return UiAction::None;
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
            if (input_.length() < kMaxInputChars) {
                const bool upper = shift_ && mode_ == KeyboardMode::Letters;
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
            mode_ = (mode_ == KeyboardMode::Letters) ? KeyboardMode::Symbols : KeyboardMode::Letters;
            shift_ = false;
            buildKeys();
            drawKeys();
            break;
        case KeyCode::Send: {
            String question = input_;
            question.trim();
            if (question.length() > 0) {
                return UiAction::Send;
            }
            break;
        }
        }
        drawInputBox();
        return UiAction::None;
    }
    return UiAction::None;
}

UiAction ChatUi::handleTouch(int16_t x, int16_t y)
{
    return screen_ == Screen::Chat ? handleChatTouch(x, y) : handleKeyboardTouch(x, y);
}
