#include "ui_icons.h"

#include <math.h>

#include "ui_common.h"

namespace ui_icons {
namespace {

using ui::rgb565;

// Every app logo is an orange disc with a white glyph and dark details.
constexpr uint16_t kDisc = ui::kColorAccent;
constexpr uint16_t kInk = ui::kColorWhite;
constexpr uint16_t kAccent = rgb565(0x1E, 0x1E, 0x22);
constexpr uint16_t kSky = rgb565(0x7F, 0xD1, 0xFF);
constexpr int16_t kDesignRadius = 30;  // glyph coordinates below are for this radius
constexpr int16_t kGearTeeth = 8;
constexpr int16_t kLabelGap = 6;
constexpr int16_t kLabelBigChars = 8;
constexpr float kPi = 3.14159265f;

struct Pen {
    Adafruit_GFX &g;
    int16_t cx, cy, r;

    int16_t s(int16_t v) const { return (int16_t)((int32_t)v * r / kDesignRadius); }
    int16_t x(int16_t v) const { return cx + s(v); }
    int16_t y(int16_t v) const { return cy + s(v); }
    void rect(int16_t x0, int16_t y0, int16_t w, int16_t h, uint16_t c) const
    {
        g.fillRect(x(x0), y(y0), s(w), s(h), c);
    }
    void round(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t rad, uint16_t c) const
    {
        g.fillRoundRect(x(x0), y(y0), s(w), s(h), max<int16_t>(1, s(rad)), c);
    }
    void dot(int16_t x0, int16_t y0, int16_t rad, uint16_t c) const
    {
        g.fillCircle(x(x0), y(y0), max<int16_t>(1, s(rad)), c);
    }
    void ring(int16_t x0, int16_t y0, int16_t rad, uint16_t c) const { g.drawCircle(x(x0), y(y0), s(rad), c); }
    void tri(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t c) const
    {
        g.fillTriangle(x(x0), y(y0), x(x1), y(y1), x(x2), y(y2), c);
    }
    // Line `thick` px wide (offset copies; fine for the short strokes used here).
    void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c, int16_t thick = 3) const
    {
        for (int16_t t = -thick / 2; t <= thick / 2; ++t) {
            g.drawLine(x(x0) + t, y(y0), x(x1) + t, y(y1), c);
            g.drawLine(x(x0), y(y0) + t, x(x1), y(y1) + t, c);
        }
    }
};

void gear(const Pen &p)
{
    for (int16_t i = 0; i < kGearTeeth; ++i) {
        const float a = i * 2 * kPi / kGearTeeth;
        const float ca = cosf(a), sa = sinf(a);
        // Rotated square tooth centered 16 px from the middle.
        const float half = 4.5f, dist = 16.0f;
        int16_t px[4], py[4];
        const float corners[4][2] = {{-half, -half}, {half, -half}, {half, half}, {-half, half}};
        for (int k = 0; k < 4; ++k) {
            const float lx = dist + corners[k][0], ly = corners[k][1];
            px[k] = (int16_t)lroundf(lx * ca - ly * sa);
            py[k] = (int16_t)lroundf(lx * sa + ly * ca);
        }
        p.tri(px[0], py[0], px[1], py[1], px[2], py[2], kInk);
        p.tri(px[0], py[0], px[2], py[2], px[3], py[3], kInk);
    }
    p.dot(0, 0, 15, kInk);
    p.dot(0, 0, 6, kDisc);
}

void face(const Pen &p)
{
    p.line(0, -20, 0, -14, kInk, 2);
    p.dot(0, -21, 3, kAccent);
    p.round(-18, -13, 36, 28, 9, kInk);
    p.round(-11, -6, 7, 11, 3, kAccent);
    p.round(4, -6, 7, 11, 3, kAccent);
    p.rect(-5, 9, 10, 2, kDisc);
}

void chat(const Pen &p)
{
    p.round(-18, -14, 36, 24, 8, kInk);
    p.tri(-10, 8, -2, 8, -12, 18, kInk);
    for (int16_t i = -1; i <= 1; ++i) {
        p.dot(i * 9, -2, 3, kAccent);
    }
}

void games(const Pen &p)
{
    p.round(-20, -11, 40, 22, 10, kInk);
    p.rect(-14, -2, 11, 4, kDisc);
    p.rect(-10, -6, 4, 12, kDisc);
    p.dot(9, -3, 3, kAccent);
    p.dot(14, 3, 3, kAccent);
}

void newApp(const Pen &p)
{
    p.round(-3, -15, 7, 30, 2, kAccent);
    p.round(-15, -3, 30, 7, 2, kAccent);
}

void clockIcon(const Pen &p)
{
    p.dot(0, 0, 18, kInk);
    p.dot(0, 0, 15, kDisc);
    p.line(0, 0, 0, -10, kInk);
    p.line(0, 0, 8, 3, kInk);
    p.dot(0, 0, 2, kAccent);
}

void timer(const Pen &p)
{
    p.rect(-12, -17, 24, 3, kInk);
    p.rect(-12, 14, 24, 3, kInk);
    p.tri(-10, -14, 10, -14, 0, 0, kInk);
    p.tri(-10, 14, 10, 14, 0, 0, kInk);
    p.tri(-5, 13, 5, 13, 0, 6, kAccent);
}

void dice(const Pen &p)
{
    p.round(-15, -15, 30, 30, 6, kInk);
    for (int16_t dx : {-7, 7}) {
        for (int16_t dy : {-7, 7}) {
            p.dot(dx, dy, 3, kDisc);
        }
    }
    p.dot(0, 0, 3, kAccent);
}

void counter(const Pen &p)
{
    p.round(-17, -13, 34, 26, 6, kInk);
    p.round(-11, -2, 11, 4, 1, kAccent);
    p.round(-7, -6, 4, 12, 1, kAccent);
    p.rect(5, -8, 4, 16, kDisc);
}

void weather(const Pen &p)
{
    p.dot(7, -7, 10, kAccent);
    p.dot(-6, 4, 9, kInk);
    p.dot(5, 1, 11, kInk);
    p.round(-15, 4, 32, 11, 5, kInk);
}

void pong(const Pen &p)
{
    p.rect(-17, -11, 4, 16, kInk);
    p.rect(13, -4, 4, 16, kInk);
    for (int16_t y = -14; y <= 12; y += 6) {
        p.rect(-1, y, 2, 3, kInk);
    }
    p.dot(-5, -4, 3, kAccent);
}

void caro(const Pen &p)
{
    for (int16_t i = -12; i <= 12; i += 8) {
        p.rect(i, -16, 1, 32, kInk);
        p.rect(-16, i, 32, 1, kInk);
    }
    for (int16_t i = -2; i <= 1; ++i) {
        p.dot(i * 8 + 4, -i * 8 - 4, 3, i % 2 == 0 ? kAccent : kInk);
    }
}

void ticTacToe(const Pen &p)
{
    p.rect(-6, -17, 2, 34, kInk);
    p.rect(5, -17, 2, 34, kInk);
    p.rect(-17, -6, 34, 2, kInk);
    p.rect(-17, 5, 34, 2, kInk);
    p.line(-15, -15, -9, -9, kAccent, 2);
    p.line(-9, -15, -15, -9, kAccent, 2);
    p.ring(0, 0, 3, kInk);
    p.ring(12, 12, 3, kInk);
    p.line(9, -15, 15, -9, kAccent, 2);
    p.line(15, -15, 9, -9, kAccent, 2);
}

void bricks(const Pen &p)
{
    for (int16_t row = 0; row < 2; ++row) {
        for (int16_t col = 0; col < 4; ++col) {
            const uint16_t c = (row + col) % 3 == 0 ? kAccent : kInk;
            p.round(-19 + col * 10 + (row % 2) * 2, -14 + row * 7, 8, 5, 2, c);
        }
    }
    p.dot(0, 3, 3, kAccent);
    p.round(-8, 12, 16, 4, 2, kInk);
}

void snake(const Pen &p)
{
    static const int8_t kBody[][2] = {{-14, 10}, {-7, 10}, {0, 10}, {0, 3}, {0, -4}, {7, -4}, {14, -4}};
    for (const auto &seg : kBody) {
        p.round(seg[0] - 3, seg[1] - 3, 7, 7, 2, kInk);
    }
    p.dot(14, -4, 4, kInk);
    p.dot(15, -5, 1, kDisc);
    p.dot(-6, -12, 3, kAccent);
}

void paint(const Pen &p)
{
    p.line(-12, 12, 10, -10, kInk, 5);
    p.tri(-16, 16, -13, 8, -8, 13, kAccent);
    p.dot(12, -12, 4, kInk);
    p.dot(-12, -10, 3, kSky);
    p.dot(12, 10, 3, kAccent);
}

void mapPin(const Pen &p)
{
    p.line(-17, 15, -4, 4, kInk, 3);
    p.line(-4, 4, 17, 13, kInk, 3);
    p.dot(0, -7, 11, kAccent);
    p.tri(-9, -2, 9, -2, 0, 15, kAccent);
    p.dot(0, -7, 4, kDisc);
}

// A pawn on a little board edge.
void chessPawn(const Pen &p)
{
    p.dot(0, -10, 6, kInk);
    p.tri(-8, 10, 8, 10, 0, -6, kInk);
    p.round(-11, 4, 22, 4, 2, kInk);
    p.round(-14, 10, 28, 6, 2, kAccent);
}

// Claude-written apps without their own icon: initials on the same orange disc as the other logos.
void letters(const Pen &p, const String &text)
{
    p.g.fillCircle(p.cx, p.cy, p.r, kDisc);
    String initials;
    bool takeNext = true;
    for (size_t i = 0; i < text.length() && initials.length() < 2; ++i) {
        const char c = text[i];
        if (c == ' ') {
            takeNext = true;
        } else if (takeNext) {
            initials += (char)toupper(c);
            takeNext = false;
        }
    }
    const uint8_t size = p.r >= 24 ? 3 : 2;
    const int16_t w = (int16_t)initials.length() * ui::kFontW * size;
    p.g.setTextSize(size);
    p.g.setTextColor(ui::kColorWhite);
    p.g.setCursor(p.cx - w / 2 + 1, p.cy - ui::kFontH * size / 2 + 1);
    p.g.print(initials);
}

}  // namespace

Icon iconForApp(const String &appId)
{
    static const struct {
        const char *id;
        Icon icon;
    } kKnown[] = {{"dong_ho", Icon::Clock},   {"hen_gio", Icon::Timer},   {"xuc_xac", Icon::Dice},
                  {"bo_dem", Icon::Counter},  {"thoi_tiet", Icon::Weather}};
    for (const auto &k : kKnown) {
        if (appId == k.id) {
            return k.icon;
        }
    }
    return Icon::Letters;
}

void draw(Adafruit_GFX &gfx, Icon icon, int16_t cx, int16_t cy, int16_t r, uint16_t color, const String &text)
{
    const Pen p{gfx, cx, cy, r};
    if (icon == Icon::Letters) {
        (void)color;  // app colors are no longer used for logos
        letters(p, text);
        return;
    }
    gfx.fillCircle(cx, cy, r, kDisc);
    switch (icon) {
    case Icon::Settings: gear(p); break;
    case Icon::Face: face(p); break;
    case Icon::Chat: chat(p); break;
    case Icon::Games: games(p); break;
    case Icon::NewApp: newApp(p); break;
    case Icon::Clock: clockIcon(p); break;
    case Icon::Timer: timer(p); break;
    case Icon::Dice: dice(p); break;
    case Icon::Counter: counter(p); break;
    case Icon::Weather: weather(p); break;
    case Icon::Pong: pong(p); break;
    case Icon::Caro: caro(p); break;
    case Icon::TicTacToe: ticTacToe(p); break;
    case Icon::Bricks: bricks(p); break;
    case Icon::Snake: snake(p); break;
    case Icon::Paint: paint(p); break;
    case Icon::Map: mapPin(p); break;
    case Icon::Chess: chessPawn(p); break;
    case Icon::Letters: break;
    }
}

void drawWithLabel(Adafruit_GFX &gfx, Icon icon, int16_t cx, int16_t cy, int16_t cellW, const String &label,
                   uint16_t color)
{
    draw(gfx, icon, cx, cy, kRadius, color, label);
    const bool big = (int16_t)label.length() <= kLabelBigChars && (int16_t)label.length() * ui::kFontW * 2 <= cellW;
    const uint8_t size = big ? 2 : 1;
    const String text = ui::ellipsize(label, (size_t)(cellW / (ui::kFontW * size)));
    ui::drawCentered(gfx, text, cx - cellW / 2, cellW, cy + kRadius + kLabelGap, size, ui::kColorText);
}

}  // namespace ui_icons
