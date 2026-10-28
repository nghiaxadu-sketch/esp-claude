// Round launcher icons drawn with GFX primitives (no bitmaps in flash): orange disc + white glyph with
// dark details, on the black home screen.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

namespace ui_icons {

enum class Icon : uint8_t {
    Letters,  // Claude-written app without its own icon: initials on the orange disc
    Settings,
    Face,
    Chat,
    Games,
    NewApp,
    Clock,
    Timer,
    Dice,
    Counter,
    Weather,
    Pong,
    Caro,
    TicTacToe,
    Bricks,
    Snake,
    Paint,
    Map,
    Chess,
};

constexpr int16_t kRadius = 30;

// Icon for a saved Claude app id (built-in samples have their own, others show initials).
Icon iconForApp(const String &appId);

// Disc of radius r centered at (cx, cy). `letters` (ASCII) are used by Icon::Letters; `color` (the app's own
// color) is ignored since every logo is orange.
void draw(Adafruit_GFX &gfx, Icon icon, int16_t cx, int16_t cy, int16_t r = kRadius, uint16_t color = 0,
          const String &letters = String());

// Icon disc with its label underneath, centered in a cell of width w; the label uses size 2 when it fits.
void drawWithLabel(Adafruit_GFX &gfx, Icon icon, int16_t cx, int16_t cy, int16_t cellW, const String &label,
                   uint16_t color = 0);

}  // namespace ui_icons
