// Base class of the built-in games. A game owns the area below the header bar; the header shows the
// "<" back arrow (handled by GamesUi), the title and a short status such as the score.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include "ui_common.h"

class Game {
public:
    explicit Game(Adafruit_GFX &gfx) : gfx_(gfx) {}
    virtual ~Game() = default;

    virtual const char *title() const = 0;
    // Resets the game and draws the whole screen (header included).
    virtual void start() = 0;
    // Called on every loop pass; games advance in fixed steps from nowMs.
    virtual void update(uint32_t nowMs) = 0;
    // Stylus/finger events in screen coordinates (taps on the back arrow never reach the game).
    virtual void press(int16_t x, int16_t y) = 0;
    virtual void drag(int16_t x, int16_t y) { (void)x, (void)y; }
    virtual void release() {}

protected:
    static constexpr int16_t kTop = ui::kHeaderH;  // first row of the play field
    static constexpr int16_t kFieldH = ui::kScreenH - kTop;

    void drawHud(const String &status)
    {
        ui::drawBackHeader(gfx_, title(), status, ui::kColorText);
    }
    // Centered two-line message in a box over the field (game over, your turn...).
    void drawBanner(const String &line1, const String &line2, uint16_t color);
    void sound(uint32_t freqHz, uint32_t ms);

    Adafruit_GFX &gfx_;
};

// Runs `step` every stepMs (at most maxSteps per call, so a long pause does not fast-forward).
class StepClock {
public:
    void reset(uint32_t nowMs) { lastMs_ = nowMs; }
    template <typename F>
    void run(uint32_t nowMs, uint32_t stepMs, F step, uint8_t maxSteps = 3)
    {
        uint8_t steps = 0;
        while (nowMs - lastMs_ >= stepMs && steps < maxSteps) {
            lastMs_ += stepMs;
            step();
            ++steps;
        }
        if (nowMs - lastMs_ >= stepMs) {
            lastMs_ = nowMs;
        }
    }

private:
    uint32_t lastMs_ = 0;
};
