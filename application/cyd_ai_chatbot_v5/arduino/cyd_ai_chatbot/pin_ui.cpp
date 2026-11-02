#include "pin_ui.h"

namespace {

using namespace ui;

constexpr int16_t kTitleY = 26;
constexpr int16_t kPromptY = 42;
constexpr int16_t kDotsY = 64;
constexpr int16_t kDotR = 7;
constexpr int16_t kDotGap = 26;
constexpr int16_t kMessageY = 82;
constexpr int16_t kPadTop = 98;
constexpr int16_t kKeyW = 76;
constexpr int16_t kKeyH = 31;  // 4 rows end at 98 + 4 * 35 - 4 = 234
constexpr int16_t kKeyGap = 4;
constexpr size_t kKeys = 12;
constexpr const char *const kLabels[kKeys] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "Xoa", "0", "Huy"};
constexpr size_t kDeleteKey = 9;
constexpr size_t kCancelKey = 11;

}  // namespace

PinUi::PinUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
}

void PinUi::show(PinMode mode, const String &current, const char *title)
{
    mode_ = mode;
    current_ = current;
    title_ = title;
    entry_ = "";
    first_ = "";
    newPin_ = "";
    step_ = mode == PinMode::Set || (mode == PinMode::Change && current.length() == 0) ? Step::New : Step::Old;
    visible_ = true;
    draw();
}

Rect PinUi::keyRect(size_t index) const
{
    const int16_t padW = 3 * kKeyW + 2 * kKeyGap;
    const int16_t x0 = (kScreenW - padW) / 2;
    return {(int16_t)(x0 + (int16_t)(index % 3) * (kKeyW + kKeyGap)), (int16_t)(kPadTop + (int16_t)(index / 3) * (kKeyH + kKeyGap)),
            kKeyW, kKeyH};
}

void PinUi::draw()
{
    gfx_.fillScreen(kColorBg);
    drawBackHeader(gfx_, "Ma PIN");
    drawCentered(gfx_, ellipsize(title_, kScreenW / kFontW), 0, kScreenW, kTitleY, 1, kColorAccent);
    for (size_t i = 0; i < kKeys; ++i) {
        const bool special = i == kDeleteKey || i == kCancelKey;
        drawButton(gfx_, keyRect(i), kLabels[i], special ? kColorKeySpecial : kColorKey, kColorWhite);
    }
    drawEntry();
}

void PinUi::drawEntry()
{
    const char *prompt = step_ == Step::Old ? (mode_ == PinMode::Verify ? "Nhap ma PIN" : "Nhap ma PIN cu")
                         : step_ == Step::New ? "Ma PIN moi (6 so)"
                                              : "Nhap lai ma PIN";
    gfx_.fillRect(0, kPromptY, kScreenW, kMessageY - kPromptY, kColorBg);
    drawCentered(gfx_, prompt, 0, kScreenW, kPromptY, 1, kColorText);
    const int16_t x0 = kScreenW / 2 - (int16_t)(kDigits - 1) * kDotGap / 2;
    for (size_t i = 0; i < kDigits; ++i) {
        const int16_t x = x0 + (int16_t)i * kDotGap;
        if (i < entry_.length()) {
            gfx_.fillCircle(x, kDotsY, kDotR, kColorAccent);
        } else {
            gfx_.drawCircle(x, kDotsY, kDotR, kColorMuted);
        }
    }
}

void PinUi::drawMessage(const String &text, uint16_t color)
{
    gfx_.fillRect(0, kMessageY, kScreenW, kFontH + 2, kColorBg);
    drawCentered(gfx_, text, 0, kScreenW, kMessageY, 1, color);
}

PinResult PinUi::complete()
{
    const String typed = entry_;
    entry_ = "";
    switch (step_) {
    case Step::Old:
        if (typed != current_) {
            drawEntry();
            drawMessage("Sai ma PIN", kColorDanger);
            return PinResult::None;
        }
        if (mode_ == PinMode::Verify) {
            visible_ = false;
            return PinResult::Accepted;
        }
        step_ = Step::New;
        break;
    case Step::New:
        first_ = typed;
        step_ = Step::Repeat;
        break;
    case Step::Repeat:
        if (typed != first_) {
            step_ = Step::New;
            drawEntry();
            drawMessage("Khong khop, thu lai", kColorDanger);
            return PinResult::None;
        }
        newPin_ = typed;
        visible_ = false;
        return PinResult::Accepted;
    }
    drawEntry();
    drawMessage("", kColorMuted);
    return PinResult::None;
}

PinResult PinUi::press(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        visible_ = false;
        return PinResult::Cancelled;
    }
    for (size_t i = 0; i < kKeys; ++i) {
        if (!keyRect(i).contains(x, y)) {
            continue;
        }
        if (i == kCancelKey) {
            visible_ = false;
            return PinResult::Cancelled;
        }
        if (i == kDeleteKey) {
            if (entry_.length() > 0) {
                entry_.remove(entry_.length() - 1);
            }
        } else if (entry_.length() < kDigits) {
            entry_ += kLabels[i];
        }
        drawEntry();
        if (entry_.length() == kDigits) {
            return complete();
        }
        drawMessage("", kColorMuted);
        return PinResult::None;
    }
    return PinResult::None;
}
