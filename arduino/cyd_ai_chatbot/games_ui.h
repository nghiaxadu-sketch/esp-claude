// "Tro choi" (game console): a menu of round game icons and the running game. The "<" in the header goes
// from a game back to the menu, and from the menu back to the home screen.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include "games.h"
#include "ui_icons.h"

enum class GamesAction : uint8_t { None, Exit };

class GamesUi {
public:
    explicit GamesUi(Adafruit_GFX &gfx);

    void show();
    bool visible() const { return visible_; }
    void hide();
    // True while a game (not the menu) is on screen.
    bool playing() const { return current_ != nullptr; }

    GamesAction press(int16_t x, int16_t y);
    void drag(int16_t x, int16_t y);
    void release();
    void loop(uint32_t nowMs);

private:
    struct Entry {
        Game *game;
        ui_icons::Icon icon;
        const char *label;
    };
    static constexpr size_t kGameCount = 6;

    void drawMenu();
    int entryAt(int16_t x, int16_t y) const;

    Adafruit_GFX &gfx_;
    PongGame pong_;
    CaroGame caro_;
    TicTacToeGame xo_;
    BricksGame bricks_;
    SnakeGame snake_;
    PaintGame paint_;
    Entry entries_[kGameCount];
    Game *current_ = nullptr;
    bool visible_ = false;
};
