#include "games.h"

namespace {

using namespace ui;

constexpr int16_t kPaletteY = kScreenH - 34;
constexpr int16_t kSwatch = 26;
constexpr int16_t kSwatchGap = 4;
constexpr int16_t kPaletteX = 4;
constexpr uint16_t kPaper = kColorWhite;
constexpr uint16_t kColors[] = {
    0x0000,                    rgb565(0xE5, 0x39, 0x35), rgb565(0xF2, 0x5C, 0x2A), rgb565(0xFD, 0xD8, 0x35),
    rgb565(0x43, 0xA0, 0x47), rgb565(0x1E, 0x88, 0xE5), rgb565(0x8E, 0x24, 0xAA), kPaper,  // last = eraser
};
constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);
constexpr int16_t kSizes[] = {1, 3, 6};
constexpr size_t kSizeCount = sizeof(kSizes) / sizeof(kSizes[0]);
constexpr Rect kSizeButton = {kPaletteX + (int16_t)kColorCount * (kSwatch + kSwatchGap), kPaletteY + 4, 34, kSwatch};
constexpr Rect kClearButton = {int16_t(kSizeButton.x + kSizeButton.w + kSwatchGap), kPaletteY + 4, 36, kSwatch};

Rect swatchRect(size_t i)
{
    return {int16_t(kPaletteX + i * (kSwatch + kSwatchGap)), kPaletteY + 4, kSwatch, kSwatch};
}

}  // namespace

void PaintGame::start()
{
    gfx_.fillScreen(kColorBlack);
    drawHud("But: keo de ve");
    gfx_.fillRect(0, kTop, kScreenW, kPaletteY - kTop, kPaper);
    drawPalette();
    drawing_ = false;
}

void PaintGame::drawPalette()
{
    gfx_.fillRect(0, kPaletteY, kScreenW, kScreenH - kPaletteY, kColorHeader);
    for (size_t i = 0; i < kColorCount; ++i) {
        const Rect r = swatchRect(i);
        gfx_.fillRoundRect(r.x, r.y, r.w, r.h, 4, kColors[i]);
        if (i + 1 == kColorCount) {  // eraser: crossed-out white swatch
            gfx_.drawLine(r.x + 4, r.y + r.h - 5, r.x + r.w - 5, r.y + 4, kColorDanger);
        }
        if (i == color_) {
            gfx_.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, 5, kColorAccent);
            gfx_.drawRoundRect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, 5, kColorAccent);
        } else {
            gfx_.drawRoundRect(r.x, r.y, r.w, r.h, 4, kColorMuted);
        }
    }
    gfx_.fillRoundRect(kSizeButton.x, kSizeButton.y, kSizeButton.w, kSizeButton.h, 4, kColorButton);
    gfx_.fillCircle(kSizeButton.x + kSizeButton.w / 2, kSizeButton.y + kSizeButton.h / 2, kSizes[sizeIndex_] + 1,
                    kColorWhite);
    drawButton(gfx_, kClearButton, "Xoa", kColorDanger, kColorWhite, 1);
}

void PaintGame::stroke(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    const int16_t size = kSizes[sizeIndex_];
    const uint16_t color = kColors[color_];
    const int16_t top = kTop + size, bottom = kPaletteY - size - 1;
    y0 = constrain(y0, top, bottom);
    y1 = constrain(y1, top, bottom);
    if (size <= 1) {
        gfx_.drawLine(x0, y0, x1, y1, color);
        return;
    }
    const int16_t dx = x1 - x0, dy = y1 - y0;
    const int16_t steps = max<int16_t>(1, max<int16_t>(abs(dx), abs(dy)) / max<int16_t>(1, size / 2));
    for (int16_t i = 0; i <= steps; ++i) {
        gfx_.fillCircle(x0 + dx * i / steps, y0 + dy * i / steps, size, color);
    }
}

void PaintGame::press(int16_t x, int16_t y)
{
    if (y >= kPaletteY) {
        for (size_t i = 0; i < kColorCount; ++i) {
            if (swatchRect(i).contains(x, y)) {
                color_ = (uint8_t)i;
                drawPalette();
                return;
            }
        }
        if (kSizeButton.contains(x, y)) {
            sizeIndex_ = (uint8_t)((sizeIndex_ + 1) % kSizeCount);
            drawPalette();
        } else if (kClearButton.contains(x, y)) {
            gfx_.fillRect(0, kTop, kScreenW, kPaletteY - kTop, kPaper);
            sound(500, 60);
        }
        return;
    }
    if (y < kTop) {
        return;
    }
    drawing_ = true;
    lastX_ = x;
    lastY_ = y;
    stroke(x, y, x, y);
}

void PaintGame::drag(int16_t x, int16_t y)
{
    if (!drawing_) {
        return;
    }
    stroke(lastX_, lastY_, x, y);
    lastX_ = x;
    lastY_ = y;
}
