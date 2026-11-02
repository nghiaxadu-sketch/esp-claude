#include "calendar_ui.h"

#include <time.h>

#include "lunar_calendar.h"

namespace {

using namespace ui;

constexpr time_t kValidEpoch = 1700000000;
constexpr int16_t kNavY = kHeaderH;
constexpr int16_t kNavH = 22;
constexpr int16_t kNavArrowW = 60;
constexpr int16_t kWeekY = kNavY + kNavH + 2;
constexpr int16_t kGridX = 2;
constexpr int16_t kGridY = kWeekY + 12;
constexpr int16_t kCellW = 45;
constexpr int16_t kCellH = 27;
constexpr int16_t kInfoY = kGridY + 6 * kCellH + 4;
constexpr uint16_t kSunday = rgb565(0xE5, 0x39, 0x35);
constexpr int kSwipeMinPx = 50;
constexpr int kFirstYear = 1900;
constexpr int kLastYear = 2199;
const char *const kWeekHeads[7] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};

}  // namespace

CalendarUi::CalendarUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
}

bool CalendarUi::today(int &d, int &m, int &y) const
{
    const time_t now = time(nullptr);
    if (now < kValidEpoch) {
        return false;
    }
    struct tm t;
    localtime_r(&now, &t);
    d = t.tm_mday;
    m = t.tm_mon + 1;
    y = t.tm_year + 1900;
    return true;
}

void CalendarUi::goToday()
{
    int d, m, y;
    if (today(d, m, y)) {
        selected_ = d;
        month_ = m;
        year_ = y;
    }
    changeMonth(0);
}

void CalendarUi::show()
{
    visible_ = true;
    page_ = Page::Month;
    goToday();
}

void CalendarUi::changeMonth(int delta)
{
    month_ += delta;
    while (month_ < 1) {
        month_ += 12;
        --year_;
    }
    while (month_ > 12) {
        month_ -= 12;
        ++year_;
    }
    year_ = constrain(year_, kFirstYear, kLastYear);
    days_ = lunar::daysInSolarMonth(month_, year_);
    firstWeekday_ = lunar::weekday(lunar::jdFromDate(1, month_, year_));
    selected_ = constrain(selected_, 1, days_);
    draw();
}

void CalendarUi::draw()
{
    if (!visible_) {
        return;
    }
    gfx_.fillScreen(kColorBg);
    if (page_ == Page::Month) {
        drawMonth();
    } else {
        drawDay();
    }
}

void CalendarUi::cellRect(int day, int16_t &x, int16_t &y) const
{
    const int index = firstWeekday_ + day - 1;
    x = kGridX + (int16_t)(index % 7) * kCellW;
    y = kGridY + (int16_t)(index / 7) * kCellH;
}

void CalendarUi::drawCell(int day)
{
    int16_t x, y;
    cellRect(day, x, y);
    const lunar::LunarDate l = lunar::fromSolar(day, month_, year_);
    const char *holiday = lunar::holiday(day, month_, l, lunar::lunarMonthLength(day, month_, year_));
    int td, tm, ty;
    const bool isToday = today(td, tm, ty) && td == day && tm == month_ && ty == year_;
    const bool isSelected = day == selected_;
    gfx_.fillRect(x, y, kCellW - 1, kCellH - 1, isSelected ? kColorHeader : kColorBg);
    if (isToday) {
        gfx_.drawRect(x, y, kCellW - 1, kCellH - 1, kColorAccent);
        gfx_.drawRect(x + 1, y + 1, kCellW - 3, kCellH - 3, kColorAccent);
    }
    const bool sunday = lunar::weekday(lunar::jdFromDate(day, month_, year_)) == 0;
    gfx_.setTextSize(2);
    gfx_.setTextColor(holiday != nullptr ? kColorAccent : (sunday ? kSunday : kColorText));
    gfx_.setCursor(x + 3, y + 3);
    gfx_.print(day);
    char lunarText[8];
    if (l.day == 1) {
        snprintf(lunarText, sizeof(lunarText), "%d/%d", l.day, l.month);
    } else {
        snprintf(lunarText, sizeof(lunarText), "%d", l.day);
    }
    gfx_.setTextSize(1);
    gfx_.setTextColor(l.day == 1 || l.day == 15 ? kColorAccent : kColorMuted);
    gfx_.setCursor(x + kCellW - 3 - (int16_t)strlen(lunarText) * kFontW, y + kCellH - 10);
    gfx_.print(lunarText);
}

void CalendarUi::drawSelectedLine()
{
    gfx_.fillRect(0, kInfoY, kScreenW, kScreenH - kInfoY, kColorBg);
    const lunar::LunarDate l = lunar::fromSolar(selected_, month_, year_);
    char dayName[24];
    lunar::dayCanChi(lunar::jdFromDate(selected_, month_, year_), dayName, sizeof(dayName));
    const char *holiday = lunar::holiday(selected_, month_, l, lunar::lunarMonthLength(selected_, month_, year_));
    char line[80];
    snprintf(line, sizeof(line), "%02d/%02d: AL %d/%d%s, ngay %s%s%s", selected_, month_, l.day, l.month,
             l.leap ? "n" : "", dayName, holiday != nullptr ? " - " : "", holiday != nullptr ? holiday : "");
    drawCentered(gfx_, ellipsize(line, kScreenW / kFontW), 0, kScreenW, kInfoY + 2, 1, kColorText);
}

void CalendarUi::drawMonth()
{
    drawBackHeader(gfx_, "Lich am", "Cham ngay: chi tiet");
    drawChevronLeft(gfx_, 20, kNavY + 4, 7, kColorText);
    drawChevronRight(gfx_, kScreenW - 30, kNavY + 4, 7, kColorText);
    const lunar::LunarDate mid = lunar::fromSolar(15, month_, year_);
    char yearName[24];
    lunar::yearCanChi(mid.year, yearName, sizeof(yearName));
    char title[40];
    snprintf(title, sizeof(title), "Thang %d/%d - %s", month_, year_, yearName);
    drawCentered(gfx_, title, kNavArrowW, kScreenW - 2 * kNavArrowW, kNavY + 4, 1, kColorAccent);
    drawCentered(gfx_, "(cham: hom nay)", kNavArrowW, kScreenW - 2 * kNavArrowW, kNavY + 13, 1, kColorMuted);
    for (int i = 0; i < 7; ++i) {
        drawCentered(gfx_, kWeekHeads[i], kGridX + i * kCellW, kCellW, kWeekY, 1, i == 0 ? kSunday : kColorMuted);
    }
    for (int d = 1; d <= days_; ++d) {
        drawCell(d);
    }
    drawSelectedLine();
}

void CalendarUi::drawDay()
{
    const int32_t jd = lunar::jdFromDate(selected_, month_, year_);
    const lunar::LunarDate l = lunar::fromSolar(selected_, month_, year_);
    drawBackHeader(gfx_, "Ngay");
    char buf[96];
    snprintf(buf, sizeof(buf), "%s, %02d/%02d/%d", lunar::weekdayName(lunar::weekday(jd)), selected_, month_, year_);
    drawCentered(gfx_, buf, 0, kScreenW, 30, 2, kColorText);
    drawCentered(gfx_, String(l.day), 0, kScreenW, 52, 5, kColorAccent);
    char yearName[24], monthName[24], dayName[24];
    lunar::yearCanChi(l.year, yearName, sizeof(yearName));
    lunar::monthCanChi(l.month, l.year, monthName, sizeof(monthName));
    lunar::dayCanChi(jd, dayName, sizeof(dayName));
    snprintf(buf, sizeof(buf), "Ngay %d thang %d%s nam %s (am lich)", l.day, l.month, l.leap ? " nhuan" : "",
             yearName);
    drawCentered(gfx_, buf, 0, kScreenW, 96, 1, kColorText);
    snprintf(buf, sizeof(buf), "Ngay %s - Thang %s - Nam %s", dayName, monthName, yearName);
    drawCentered(gfx_, buf, 0, kScreenW, 112, 1, kColorMuted);
    snprintf(buf, sizeof(buf), "Tiet %s", lunar::solarTermName(lunar::solarTerm(jd)));
    drawCentered(gfx_, buf, 0, kScreenW, 128, 1, kColorMuted);

    drawCentered(gfx_, "Gio hoang dao", 0, kScreenW, 148, 1, kColorAccent);
    int hours[6];
    const size_t n = lunar::hoangDaoHours(jd, hours);
    String row1, row2;
    for (size_t i = 0; i < n; ++i) {
        char range[12];
        lunar::chiHourRange(hours[i], range, sizeof(range));
        String item = String(lunar::chiName(hours[i])) + " (" + range + ")";
        String &row = i < 3 ? row1 : row2;
        if (row.length() > 0) {
            row += "  ";
        }
        row += item;
    }
    drawCentered(gfx_, row1, 0, kScreenW, 162, 1, kColorText);
    drawCentered(gfx_, row2, 0, kScreenW, 176, 1, kColorText);

    const char *holiday = lunar::holiday(selected_, month_, l, lunar::lunarMonthLength(selected_, month_, year_));
    if (holiday != nullptr) {
        drawCentered(gfx_, holiday, 0, kScreenW, 196, 2, kColorSystem);
    }
    drawButton(gfx_, footerSlot(0, 3), "< Truoc", kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(1, 3), "Thang", kColorAccent, kColorWhite);
    drawButton(gfx_, footerSlot(2, 3), "Sau >", kColorButton, kColorWhite);
}

CalendarAction CalendarUi::back()
{
    if (page_ == Page::Day) {
        page_ = Page::Month;
        draw();
        return CalendarAction::None;
    }
    return CalendarAction::Exit;
}

void CalendarUi::swipe(int16_t dx)
{
    if (page_ != Page::Month || abs(dx) < kSwipeMinPx) {
        return;
    }
    changeMonth(dx < 0 ? 1 : -1);
}

CalendarAction CalendarUi::press(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        return back();
    }
    if (page_ == Page::Day) {
        if (footerSlot(1, 3).contains(x, y)) {
            page_ = Page::Month;
            draw();
        } else if (footerSlot(0, 3).contains(x, y) || footerSlot(2, 3).contains(x, y)) {
            const int delta = footerSlot(0, 3).contains(x, y) ? -1 : 1;
            int d, m, yy;
            lunar::jdToDate(lunar::jdFromDate(selected_, month_, year_) + delta, d, m, yy);
            selected_ = d;
            month_ = m;
            year_ = yy;
            days_ = lunar::daysInSolarMonth(month_, year_);
            firstWeekday_ = lunar::weekday(lunar::jdFromDate(1, month_, year_));
            draw();
        }
        return CalendarAction::None;
    }
    if (y >= kNavY && y < kNavY + kNavH + 2) {
        if (x < kNavArrowW) {
            changeMonth(-1);
        } else if (x >= kScreenW - kNavArrowW) {
            changeMonth(1);
        } else {
            goToday();
        }
        return CalendarAction::None;
    }
    if (y >= kGridY && y < kGridY + 6 * kCellH && x >= kGridX && x < kGridX + 7 * kCellW) {
        const int index = (y - kGridY) / kCellH * 7 + (x - kGridX) / kCellW;
        const int day = index - firstWeekday_ + 1;
        if (day >= 1 && day <= days_) {
            if (day == selected_) {
                page_ = Page::Day;  // second tap on the same day opens its page
                draw();
            } else {
                const int previous = selected_;
                selected_ = day;
                drawCell(previous);
                drawCell(day);
                drawSelectedLine();
            }
        }
    }
    return CalendarAction::None;
}

void CalendarUi::loop(uint32_t)
{
    int d, m, y;
    if (visible_ && page_ == Page::Month && today(d, m, y) && d != lastTodayDay_) {
        const bool first = lastTodayDay_ < 0;
        lastTodayDay_ = d;
        if (!first && m == month_ && y == year_) {
            draw();  // midnight: move the "today" outline
        }
    }
}
