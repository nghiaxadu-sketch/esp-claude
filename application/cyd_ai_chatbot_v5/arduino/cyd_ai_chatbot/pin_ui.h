// 6-digit PIN pad: unlocking locked apps / the AI profile, and setting or changing the PIN
// ("Dat ma PIN" -> enter twice; "Doi ma PIN" -> old PIN, then the new one twice).
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include "ui_common.h"

enum class PinMode : uint8_t { Verify, Set, Change };
enum class PinResult : uint8_t { None, Accepted, Cancelled };

class PinUi {
public:
    static constexpr size_t kDigits = 6;

    explicit PinUi(Adafruit_GFX &gfx);

    // `current` is the saved PIN ("" when none). The title says what the PIN unlocks.
    void show(PinMode mode, const String &current, const char *title);
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }

    PinResult press(int16_t x, int16_t y);
    // The new PIN after Set / Change was accepted.
    const String &newPin() const { return newPin_; }

private:
    enum class Step : uint8_t { Old, New, Repeat };

    void draw();
    void drawEntry();
    void drawMessage(const String &text, uint16_t color);
    ui::Rect keyRect(size_t index) const;
    PinResult complete();

    Adafruit_GFX &gfx_;
    PinMode mode_ = PinMode::Verify;
    Step step_ = Step::Old;
    String current_;
    String entry_;
    String first_;
    String newPin_;
    String title_;
    bool visible_ = false;
};
