// Home screen: a grid of saved apps plus Chat / new app / Wi-Fi buttons, and the per-app menu.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <vector>

#include "ui_common.h"

struct AppTile {
    String id;
    String name;  // ASCII
    uint16_t color;
};

enum class LauncherAction : uint8_t { None, OpenApp, AppMenu, Chat, NewApp, Wifi };
enum class MenuAction : uint8_t { None, Run, Edit, Delete, Cancel };

class LauncherUi {
public:
    explicit LauncherUi(Adafruit_GFX &gfx);

    void show(const std::vector<AppTile> &tiles);
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }
    void setStatus(const String &status);
    // One line above the footer (hints, last result); ASCII.
    void setNotice(const String &notice);

    // Tap released quickly: open a tile or a footer button.
    LauncherAction handleTap(int16_t x, int16_t y);
    // Long press on a tile: opens the app menu.
    LauncherAction handleLongPress(int16_t x, int16_t y);
    // Index into the tiles passed to show() for OpenApp / AppMenu.
    size_t selected() const { return selected_; }

    void showMenu(const AppTile &tile);
    bool menuVisible() const { return menuVisible_; }
    MenuAction handleMenuTap(int16_t x, int16_t y);

private:
    static constexpr size_t kCols = 3;
    static constexpr size_t kRows = 2;
    static constexpr size_t kPerPage = kCols * kRows;
    static constexpr size_t kFooterButtons = 4;
    static constexpr size_t kMenuButtons = 4;

    void draw();
    void drawNotice();
    ui::Rect tileRect(size_t slot) const;
    int tileAt(int16_t x, int16_t y) const;
    size_t pageCount() const;

    Adafruit_GFX &gfx_;
    std::vector<AppTile> tiles_;
    bool visible_ = false;
    bool menuVisible_ = false;
    size_t page_ = 0;
    size_t selected_ = 0;
    String status_;
    String notice_;
    ui::Rect footer_[kFooterButtons];
    ui::Rect menuButtons_[kMenuButtons];
};
