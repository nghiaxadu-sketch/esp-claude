#include "ui_common.h"

#include "text_utils.h"

namespace ui {
namespace {

constexpr int16_t kSplashTitleY = 80;
constexpr int16_t kSplashLineY = 116;
constexpr int16_t kSplashLineGap = 16;
constexpr int16_t kHeaderTitleX = 6;
constexpr int16_t kButtonRadius = 4;
constexpr int16_t kSlotGap = 2;
constexpr int16_t kChevronThick = 3;
constexpr int16_t kHeaderChevronX = 8;
constexpr int16_t kHeaderChevronY = 4;
constexpr int16_t kHeaderChevronSize = 7;
constexpr int16_t kBackTitleX = 26;
constexpr int16_t kBadgeX = 15;
constexpr int16_t kBadgeY = 15;
constexpr int16_t kBadgeR = 12;
constexpr uint16_t kBadgeColor = rgb565(0x30, 0x30, 0x36);

}  // namespace

Rect footerSlot(size_t index, size_t count)
{
    const int16_t w = kScreenW / count;
    return {int16_t(index * w + kSlotGap), int16_t(kScreenH - kFooterH + kSlotGap), int16_t(w - 2 * kSlotGap),
            int16_t(kFooterH - 2 * kSlotGap)};
}

String toAscii(const String &utf8)
{
    return String(text_utils::utf8ToAscii(utf8.c_str()).c_str());
}

String ellipsize(const String &ascii, size_t maxChars)
{
    if (ascii.length() <= maxChars) {
        return ascii;
    }
    return maxChars > 3 ? ascii.substring(0, maxChars - 3) + "..." : ascii.substring(0, maxChars);
}

void drawButton(Adafruit_GFX &gfx, const Rect &r, const char *label, uint16_t bg, uint16_t fg, uint8_t textSize)
{
    gfx.fillRoundRect(r.x, r.y, r.w, r.h, kButtonRadius, bg);
    gfx.setTextSize(textSize);
    gfx.setTextColor(fg);
    // Fall back to the small font when the label does not fit.
    if ((int16_t)strlen(label) * kFontW * textSize > r.w - 2 && textSize > 1) {
        textSize = 1;
        gfx.setTextSize(1);
    }
    const int16_t textW = (int16_t)strlen(label) * kFontW * textSize;
    const int16_t textH = kFontH * textSize;
    gfx.setCursor(r.x + (r.w - textW) / 2, r.y + (r.h - textH) / 2 + 1);
    gfx.print(label);
}

void drawHeader(Adafruit_GFX &gfx, const char *title, const String &right, uint16_t rightColor)
{
    gfx.fillRect(0, 0, kScreenW, kHeaderH, kColorHeader);
    gfx.setTextWrap(false);
    gfx.setTextSize(2);
    gfx.setTextColor(kColorAccent);
    gfx.setCursor(kHeaderTitleX, 3);
    gfx.print(title);
    if (right.length() > 0) {
        gfx.setTextSize(1);
        gfx.setTextColor(rightColor);
        gfx.setCursor(kScreenW - (int16_t)right.length() * kFontW - kHeaderTitleX, 8);
        gfx.print(right);
    }
}

void drawChevronLeft(Adafruit_GFX &gfx, int16_t x, int16_t y, int16_t size, uint16_t color)
{
    for (int16_t t = 0; t < kChevronThick; ++t) {
        gfx.drawLine(x + size + t, y, x + t, y + size, color);
        gfx.drawLine(x + t, y + size, x + size + t, y + 2 * size, color);
    }
}

void drawChevronRight(Adafruit_GFX &gfx, int16_t x, int16_t y, int16_t size, uint16_t color)
{
    for (int16_t t = 0; t < kChevronThick; ++t) {
        gfx.drawLine(x + t, y, x + size + t, y + size, color);
        gfx.drawLine(x + size + t, y + size, x + t, y + 2 * size, color);
    }
}

void drawBackHeader(Adafruit_GFX &gfx, const char *title, const String &right, uint16_t rightColor)
{
    gfx.fillRect(0, 0, kScreenW, kHeaderH, kColorHeader);
    drawChevronLeft(gfx, kHeaderChevronX, kHeaderChevronY, kHeaderChevronSize, kColorWhite);
    gfx.setTextWrap(false);
    gfx.setTextSize(2);
    gfx.setTextColor(kColorAccent);
    gfx.setCursor(kBackTitleX, 3);
    gfx.print(title);
    if (right.length() > 0) {
        gfx.setTextSize(1);
        gfx.setTextColor(rightColor);
        gfx.setCursor(kScreenW - (int16_t)right.length() * kFontW - kHeaderTitleX, 8);
        gfx.print(right);
    }
}

void drawBackBadge(Adafruit_GFX &gfx)
{
    gfx.fillCircle(kBadgeX, kBadgeY, kBadgeR, kBadgeColor);
    gfx.drawCircle(kBadgeX, kBadgeY, kBadgeR, kColorMuted);
    drawChevronLeft(gfx, kBadgeX - 5, kBadgeY - 6, 6, kColorWhite);
}

void drawCentered(Adafruit_GFX &gfx, const String &text, int16_t x, int16_t w, int16_t y, uint8_t size,
                  uint16_t color)
{
    gfx.setTextWrap(false);
    gfx.setTextSize(size);
    gfx.setTextColor(color);
    gfx.setCursor(x + (w - (int16_t)text.length() * kFontW * size) / 2, y);
    gfx.print(text);
}

void drawSplash(Adafruit_GFX &gfx, const char *title, const char *line1, const char *line2)
{
    gfx.fillScreen(kColorBg);
    gfx.setTextWrap(false);
    gfx.setTextSize(2);
    gfx.setTextColor(kColorAccent);
    gfx.setCursor(max(0, (kScreenW - (int16_t)strlen(title) * kFontW * 2) / 2), kSplashTitleY);
    gfx.print(title);
    gfx.setTextSize(1);
    gfx.setTextColor(kColorText);
    const char *lines[] = {line1, line2};
    int16_t y = kSplashLineY;
    for (const char *line : lines) {
        if (line != nullptr) {
            gfx.setCursor(max(0, (kScreenW - (int16_t)strlen(line) * kFontW) / 2), y);
            gfx.print(line);
            y += kSplashLineGap;
        }
    }
}

}  // namespace ui
