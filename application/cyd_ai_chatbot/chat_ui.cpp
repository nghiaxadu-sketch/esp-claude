#include "chat_ui.h"

#include <string>

#include "config.h"
#include "text_utils.h"

namespace {

using namespace ui;

constexpr int16_t kScrollbarW = 4;
constexpr int16_t kBodyTop = kHeaderH + 2;
constexpr int16_t kBodyBottom = kScreenH - kFooterH - 2;
constexpr int16_t kLineGap = 2;
constexpr size_t kMaxChatLines = 300;

constexpr const char *kTitle = "Claude Chat";

uint16_t colorFor(Speaker who)
{
    switch (who) {
    case Speaker::User:
        return kColorUser;
    case Speaker::Claude:
        return kColorText;
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

ChatUi::ChatUi(Adafruit_GFX &tft) : tft_(tft)
{
}

void ChatUi::begin()
{
    lineHeight_ = kFontH * CHAT_TEXT_SIZE + kLineGap;
    visibleLines_ = (kBodyBottom - kBodyTop) / lineHeight_;
    charsPerLine_ = (kScreenW - 2 * kMargin - kScrollbarW - 2) / (kFontW * CHAT_TEXT_SIZE);
    for (size_t i = 0; i < kFooterButtons; ++i) {
        footer_[i] = footerSlot(i, kFooterButtons);
    }
}

void ChatUi::drawHeader()
{
    if (busy_) {
        ui::drawHeader(tft_, kTitle, "Dang nghi...", kColorSystem);
    } else {
        ui::drawHeader(tft_, kTitle, status_);
    }
}

// ---------------------------------------------------------------- chat screen

void ChatUi::show()
{
    visible_ = true;
    tft_.fillScreen(kColorBg);
    tft_.setTextWrap(false);
    drawHeader();
    drawChatBody();
    drawFooter();
}

void ChatUi::drawChatBody()
{
    if (!visible_) {
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
    static const char *const kLabels[kFooterButtons] = {"Ve", "Len", "Xuong", "Nhap", "Xoa"};
    for (size_t i = 0; i < kFooterButtons; ++i) {
        const bool primary = (i == 3);
        drawButton(tft_, footer_[i], kLabels[i], primary ? kColorAccent : kColorButton, kColorWhite);
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
    if (visible_) {
        drawHeader();
    }
}

void ChatUi::setBusy(bool busy)
{
    busy_ = busy;
    if (visible_) {
        drawHeader();
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

ChatAction ChatUi::handleTouch(int16_t x, int16_t y)
{
    const int page = max(1, visibleLines_ - 2);
    if (footer_[0].contains(x, y)) {
        return ChatAction::Home;
    }
    if (footer_[1].contains(x, y)) {
        scrollBy(-page);
    } else if (footer_[2].contains(x, y)) {
        scrollBy(page);
    } else if (footer_[3].contains(x, y)) {
        return ChatAction::Compose;
    } else if (footer_[4].contains(x, y)) {
        return ChatAction::ClearChat;
    } else if (y >= kBodyTop && y < kBodyBottom) {
        // Tap on the upper/lower half of the chat also scrolls.
        scrollBy(y < (kBodyTop + kBodyBottom) / 2 ? -page : page);
    }
    return ChatAction::None;
}
