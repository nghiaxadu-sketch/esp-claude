// Shared look of every screen: colors, geometry and small drawing helpers.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

namespace ui {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t kColorBg = rgb565(0x16, 0x16, 0x18);
constexpr uint16_t kColorHeader = rgb565(0x2A, 0x2A, 0x2E);
constexpr uint16_t kColorAccent = rgb565(0xD9, 0x77, 0x57);  // Claude orange
constexpr uint16_t kColorUser = rgb565(0x7F, 0xD1, 0xFF);
constexpr uint16_t kColorText = rgb565(0xF0, 0xED, 0xE6);
constexpr uint16_t kColorSystem = rgb565(0xFF, 0xC8, 0x57);
constexpr uint16_t kColorMuted = rgb565(0x8A, 0x8A, 0x90);
constexpr uint16_t kColorButton = rgb565(0x3A, 0x3A, 0x40);
constexpr uint16_t kColorKey = rgb565(0x33, 0x33, 0x38);
constexpr uint16_t kColorKeySpecial = rgb565(0x4A, 0x4A, 0x52);
constexpr uint16_t kColorDanger = rgb565(0xC6, 0x28, 0x28);
constexpr uint16_t kColorOk = rgb565(0x2E, 0x7D, 0x32);
constexpr uint16_t kColorWhite = 0xFFFF;

constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr int16_t kHeaderH = 22;
constexpr int16_t kFooterH = 36;
constexpr int16_t kMargin = 4;
constexpr int16_t kFontW = 6;  // built-in 5x7 font cell at text size 1
constexpr int16_t kFontH = 8;

struct Rect {
    int16_t x, y, w, h;
    bool contains(int16_t px, int16_t py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// Footer split into `count` equal buttons; returns rectangle `index`.
Rect footerSlot(size_t index, size_t count);

// UTF-8 to printable ASCII for the built-in font (Vietnamese loses its diacritics).
String toAscii(const String &utf8);
// Cuts ASCII text to maxChars, ending with "..." when shortened.
String ellipsize(const String &ascii, size_t maxChars);

void drawButton(Adafruit_GFX &gfx, const Rect &r, const char *label, uint16_t bg, uint16_t fg, uint8_t textSize = 2);
// Title on the left, optional ASCII status text on the right.
void drawHeader(Adafruit_GFX &gfx, const char *title, const String &right = String(),
                uint16_t rightColor = kColorMuted);
// Full-screen message used while booting, connecting or waiting for Claude.
void drawSplash(Adafruit_GFX &gfx, const char *title, const char *line1, const char *line2 = nullptr);

}  // namespace ui
