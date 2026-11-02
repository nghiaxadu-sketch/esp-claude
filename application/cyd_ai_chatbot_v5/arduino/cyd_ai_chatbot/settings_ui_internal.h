// Layout shared by settings_ui.cpp and settings_pages.cpp (private to the Settings screen).
#pragma once

#include "ui_common.h"

namespace settings_layout {

constexpr int16_t kRowH = 36;
constexpr int16_t kRowIconX = 20;
constexpr int16_t kRowLabelX = 40;
constexpr int16_t kRowChevronX = 302;
constexpr int16_t kRowChevronSize = 5;
constexpr int16_t kRowValueRight = 294;

// Option rows on detail pages (label left, value right).
constexpr int16_t kOptionTop = ui::kHeaderH + 4;
constexpr int16_t kOptionH = 30;
constexpr size_t kOptionRows = 6;  // y 26..206, above the footer buttons

// Segmented slider (brightness, volume).
constexpr int16_t kSliderX = 20;
constexpr int16_t kSliderY = 62;
constexpr int16_t kSliderH = 46;
constexpr int16_t kSegments = 10;
constexpr int16_t kSegmentGap = 6;
constexpr int16_t kSegmentW = (ui::kScreenW - 2 * kSliderX - (kSegments - 1) * kSegmentGap) / kSegments;
constexpr int16_t kSliderW = kSegments * kSegmentW + (kSegments - 1) * kSegmentGap;
constexpr int16_t kSliderHitPad = 10;

// Draws the slider with `percent` lit and the big value on the right of `label`.
void drawSlider(Adafruit_GFX &gfx, const char *label, uint8_t percent);
// Slider value for a stylus x position, in `step` steps, at least `minimum`.
uint8_t sliderValue(int16_t x, uint8_t step, uint8_t minimum);
bool onSlider(int16_t y);

}  // namespace settings_layout
