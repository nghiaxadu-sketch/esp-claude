#include "game.h"

#include "board_hw.h"

namespace {

constexpr int16_t kBannerW = 240;
constexpr int16_t kBannerH = 56;
constexpr int16_t kBannerRadius = 8;

}  // namespace

void Game::drawBanner(const String &line1, const String &line2, uint16_t color)
{
    const int16_t x = (ui::kScreenW - kBannerW) / 2;
    const int16_t y = kTop + (kFieldH - kBannerH) / 2;
    gfx_.fillRoundRect(x, y, kBannerW, kBannerH, kBannerRadius, ui::kColorHeader);
    gfx_.drawRoundRect(x, y, kBannerW, kBannerH, kBannerRadius, color);
    ui::drawCentered(gfx_, line1, x, kBannerW, y + 10, 2, color);
    ui::drawCentered(gfx_, line2, x, kBannerW, y + 36, 1, ui::kColorText);
}

void Game::sound(uint32_t freqHz, uint32_t ms)
{
    board_hw::beep(freqHz, ms);
}
