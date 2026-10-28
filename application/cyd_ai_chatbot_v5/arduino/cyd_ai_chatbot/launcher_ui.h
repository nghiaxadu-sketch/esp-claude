// Home screen: round icons for the built-in screens (Settings, Claude face, Chat, Games, Map, + App) followed by
// the saved Claude apps, 3x2 per page; swipe or tap the arrows to change page. Long press an app for its menu.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <vector>

#include "ui_common.h"
#include "ui_icons.h"

struct AppTile {
    String id;
    String name;  // ASCII
    uint16_t color;
};

enum class LauncherAction : uint8_t { None, OpenApp, AppMenu, Chat, NewApp, Settings, Face, Games, Map };
enum class MenuAction : uint8_t { None, Run, Edit, Delete, Cancel };

class LauncherUi {
public:
    explicit LauncherUi(Adafruit_GFX &gfx);

    void show(const std::vector<AppTile> &tiles);
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }
    // Clock on the left of the top line, short status (Wi-Fi) on the right; ASCII.
    void setStatus(const String &clock, const String &status);
    // One line above the page dots (hints, last result); ASCII.
    void setNotice(const String &notice);

    // Tap released quickly: open an icon or change page.
    LauncherAction handleTap(int16_t x, int16_t y);
    // Long press on an app icon: opens the app menu.
    LauncherAction handleLongPress(int16_t x, int16_t y);
    // Horizontal stylus/finger swipe of dx pixels (negative = to the left = next page).
    void handleSwipe(int16_t dx);
    // Index into the tiles passed to show() for OpenApp / AppMenu.
    size_t selected() const { return selected_; }

    void showMenu(const AppTile &tile);
    bool menuVisible() const { return menuVisible_; }
    MenuAction handleMenuTap(int16_t x, int16_t y);

private:
    struct Entry {
        ui_icons::Icon icon;
        LauncherAction action;
        const char *label;  // built-in entries
        int appIndex;       // >= 0 for saved apps
    };

    static constexpr size_t kCols = 3;
    static constexpr size_t kRows = 2;
    static constexpr size_t kPerPage = kCols * kRows;
    static constexpr size_t kMenuButtons = 4;

    void draw();
    void drawTopLine();
    void drawNotice();
    void drawPager();
    void goToPage(size_t page);
    void cellCenter(size_t slot, int16_t &cx, int16_t &cy) const;
    int entryAt(int16_t x, int16_t y) const;
    size_t pageCount() const;

    Adafruit_GFX &gfx_;
    std::vector<AppTile> tiles_;
    std::vector<Entry> entries_;
    bool visible_ = false;
    bool menuVisible_ = false;
    size_t page_ = 0;
    size_t selected_ = 0;
    String clock_;
    String status_;
    String notice_;
    ui::Rect prevButton_;
    ui::Rect nextButton_;
    ui::Rect menuButtons_[kMenuButtons];
};
