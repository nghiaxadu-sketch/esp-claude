#include "games_ui.h"

namespace {

using namespace ui;

constexpr int16_t kCellW = kScreenW / 3;
constexpr int16_t kRowCenterY[2] = {72, 160};
constexpr int16_t kCellHalfH = 42;
constexpr size_t kCols = 3;

}  // namespace

GamesUi::GamesUi(Adafruit_GFX &gfx)
    : gfx_(gfx),
      pong_(gfx),
      caro_(gfx),
      xo_(gfx),
      bricks_(gfx),
      snake_(gfx),
      paint_(gfx),
      entries_{{&pong_, ui_icons::Icon::Pong, "Pong"},       {&caro_, ui_icons::Icon::Caro, "Caro"},
               {&xo_, ui_icons::Icon::TicTacToe, "Co XO"},   {&bricks_, ui_icons::Icon::Bricks, "Pha gach"},
               {&snake_, ui_icons::Icon::Snake, "Con ran"}, {&paint_, ui_icons::Icon::Paint, "Ve tranh"}}
{
}

void GamesUi::show()
{
    visible_ = true;
    current_ = nullptr;
    drawMenu();
}

void GamesUi::hide()
{
    visible_ = false;
    current_ = nullptr;
}

void GamesUi::drawMenu()
{
    gfx_.fillScreen(kColorBlack);
    drawBackHeader(gfx_, "Tro choi", "Pong, Caro, XO: dau voi may");
    for (size_t i = 0; i < kGameCount; ++i) {
        const int16_t cx = (int16_t)(kCellW / 2 + (i % kCols) * kCellW);
        ui_icons::drawWithLabel(gfx_, entries_[i].icon, cx, kRowCenterY[i / kCols], kCellW, entries_[i].label);
    }
}

int GamesUi::entryAt(int16_t x, int16_t y) const
{
    for (size_t i = 0; i < kGameCount; ++i) {
        const int16_t cx = (int16_t)(kCellW / 2 + (i % kCols) * kCellW);
        const Rect cell = {int16_t(cx - kCellW / 2), int16_t(kRowCenterY[i / kCols] - kCellHalfH), kCellW,
                           int16_t(2 * kCellHalfH)};
        if (cell.contains(x, y)) {
            return (int)i;
        }
    }
    return -1;
}

GamesAction GamesUi::press(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        if (current_ == nullptr) {
            return GamesAction::Exit;
        }
        current_ = nullptr;
        drawMenu();
        return GamesAction::None;
    }
    if (current_ != nullptr) {
        current_->press(x, y);
        return GamesAction::None;
    }
    const int i = entryAt(x, y);
    if (i >= 0) {
        current_ = entries_[i].game;
        current_->start();
    }
    return GamesAction::None;
}

void GamesUi::drag(int16_t x, int16_t y)
{
    if (current_ != nullptr) {
        current_->drag(x, y);
    }
}

void GamesUi::release()
{
    if (current_ != nullptr) {
        current_->release();
    }
}

void GamesUi::loop(uint32_t nowMs)
{
    if (current_ != nullptr) {
        current_->update(nowMs);
    }
}
