// "Lich am": a month grid with the solar day and the lunar day in every cell (holidays and Sundays
// highlighted, today outlined), and a day page with the full lunar date, can chi of the year / month / day,
// the solar term (tiet khi), the auspicious hours (gio hoang dao) and the holiday.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include "ui_common.h"

enum class CalendarAction : uint8_t { None, Exit };

class CalendarUi {
public:
    explicit CalendarUi(Adafruit_GFX &gfx);

    void show();
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }

    CalendarAction press(int16_t x, int16_t y);
    CalendarAction back();
    // Horizontal swipe on the month grid: next / previous month.
    void swipe(int16_t dx);
    void loop(uint32_t nowMs);

private:
    enum class Page : uint8_t { Month, Day };

    void draw();
    void drawMonth();
    void drawCell(int day);
    void drawSelectedLine();
    void drawDay();
    void goToday();
    void changeMonth(int delta);
    bool today(int &d, int &m, int &y) const;
    void cellRect(int day, int16_t &x, int16_t &y) const;

    Adafruit_GFX &gfx_;
    Page page_ = Page::Month;
    bool visible_ = false;
    int month_ = 1;
    int year_ = 2026;
    int selected_ = 1;      // day of month_
    int firstWeekday_ = 0;  // weekday of day 1 (0 = Sunday)
    int days_ = 31;
    int lastTodayDay_ = -1;
};
