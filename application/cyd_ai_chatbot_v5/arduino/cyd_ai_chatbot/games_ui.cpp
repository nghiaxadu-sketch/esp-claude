#include "games_ui.h"

namespace {

using namespace ui;

constexpr size_t kCols = 4;
constexpr int16_t kCellW = kScreenW / kCols;
constexpr int16_t kRowCenterY[2] = {72, 160};
constexpr int16_t kCellHalfH = 42;

constexpr int16_t kChoiceX = 20;
constexpr int16_t kChoiceW = kScreenW - 2 * kChoiceX;
constexpr int16_t kChoiceH = 56;
constexpr Rect kOnePlayerButton = {kChoiceX, 50, kChoiceW, kChoiceH};
constexpr Rect kTwoPlayersButton = {kChoiceX, 122, kChoiceW, kChoiceH};
constexpr int16_t kChoiceHintY = 196;

}  // namespace

GamesUi::GamesUi(Adafruit_GFX &gfx)
    : gfx_(gfx),
      pong_(gfx),
      caro_(gfx),
      xo_(gfx),
      chess_(gfx),
      bricks_(gfx),
      snake_(gfx),
      paint_(gfx),
      entries_{{&pong_, ui_icons::Icon::Pong, "Pong"},         {&caro_, ui_icons::Icon::Caro, "Caro"},
               {&xo_, ui_icons::Icon::TicTacToe, "Co XO"},     {&chess_, ui_icons::Icon::Chess, "Co vua"},
               {&bricks_, ui_icons::Icon::Bricks, "Pha gach"}, {&snake_, ui_icons::Icon::Snake, "Con ran"},
               {&paint_, ui_icons::Icon::Paint, "Ve tranh"}}
{
}

void GamesUi::show()
{
    visible_ = true;
    current_ = nullptr;
    choosing_ = nullptr;
    drawMenu();
}

void GamesUi::hide()
{
    visible_ = false;
    current_ = nullptr;
    choosing_ = nullptr;
}

void GamesUi::drawMenu()
{
    gfx_.fillScreen(kColorBlack);
    drawBackHeader(gfx_, "Tro choi", "Choi 1 hoac 2 nguoi");
    for (size_t i = 0; i < kGameCount; ++i) {
        const int16_t cx = (int16_t)(kCellW / 2 + (i % kCols) * kCellW);
        ui_icons::drawWithLabel(gfx_, entries_[i].icon, cx, kRowCenterY[i / kCols], kCellW, entries_[i].label);
    }
}

void GamesUi::drawPlayerChoice()
{
    gfx_.fillScreen(kColorBlack);
    drawBackHeader(gfx_, choosing_->title());
    drawButton(gfx_, kOnePlayerButton, "1 nguoi (dau may)", kColorAccent, kColorWhite);
    drawButton(gfx_, kTwoPlayersButton, "2 nguoi", kColorButton, kColorWhite);
    drawCentered(gfx_, "2 nguoi: thay phien cham tren cung 1 man hinh", 0, kScreenW, kChoiceHintY, 1, kColorMuted);
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

void GamesUi::launch(Game *game, bool twoPlayers)
{
    choosing_ = nullptr;
    current_ = game;
    current_->setTwoPlayers(twoPlayers);
    current_->start();
}

GamesAction GamesUi::press(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        if (current_ == nullptr && choosing_ == nullptr) {
            return GamesAction::Exit;
        }
        current_ = nullptr;
        choosing_ = nullptr;
        drawMenu();
        return GamesAction::None;
    }
    if (current_ != nullptr) {
        current_->press(x, y);
        return GamesAction::None;
    }
    if (choosing_ != nullptr) {
        if (kOnePlayerButton.contains(x, y)) {
            launch(choosing_, false);
        } else if (kTwoPlayersButton.contains(x, y)) {
            launch(choosing_, true);
        }
        return GamesAction::None;
    }
    const int i = entryAt(x, y);
    if (i < 0) {
        return GamesAction::None;
    }
    Game *game = entries_[i].game;
    if (game->supportsTwoPlayers()) {
        choosing_ = game;
        drawPlayerChoice();
    } else {
        launch(game, false);
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
