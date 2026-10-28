#include "launcher_ui.h"

namespace {

using namespace ui;
using ui_icons::Icon;

constexpr int16_t kTopLineH = 24;
constexpr int16_t kCellW = kScreenW / 3;
constexpr int16_t kRowCenterY[2] = {64, 150};
constexpr int16_t kCellHalfH = 42;  // tap area above/below a row center (icon + label)
constexpr int16_t kNoticeY = 206;
constexpr int16_t kPagerY = 226;
constexpr int16_t kPagerDotR = 3;
constexpr int16_t kPagerDotGap = 14;
constexpr int16_t kArrowW = 44;
constexpr int16_t kArrowTop = 200;
constexpr int16_t kArrowSize = 8;
constexpr int16_t kSwipeMin = 50;

constexpr int16_t kMenuX = 30;
constexpr int16_t kMenuY = 40;
constexpr int16_t kMenuW = kScreenW - 2 * kMenuX;
constexpr int16_t kMenuH = 160;
constexpr int16_t kMenuButtonH = 40;
constexpr int16_t kMenuGap = 6;
constexpr int16_t kMenuRadius = 8;

}  // namespace

LauncherUi::LauncherUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
    prevButton_ = {0, kArrowTop, kArrowW, int16_t(kScreenH - kArrowTop)};
    nextButton_ = {int16_t(kScreenW - kArrowW), kArrowTop, kArrowW, int16_t(kScreenH - kArrowTop)};
    const int16_t bw = (kMenuW - 3 * kMenuGap) / 2;
    const int16_t row1 = kMenuY + kMenuH - 2 * kMenuButtonH - 2 * kMenuGap;
    const int16_t row2 = row1 + kMenuButtonH + kMenuGap;
    menuButtons_[0] = {int16_t(kMenuX + kMenuGap), row1, bw, kMenuButtonH};           // Run
    menuButtons_[1] = {int16_t(kMenuX + 2 * kMenuGap + bw), row1, bw, kMenuButtonH};  // Edit
    menuButtons_[2] = {int16_t(kMenuX + kMenuGap), row2, bw, kMenuButtonH};           // Delete
    menuButtons_[3] = {int16_t(kMenuX + 2 * kMenuGap + bw), row2, bw, kMenuButtonH};  // Cancel
}

size_t LauncherUi::pageCount() const
{
    return entries_.empty() ? 1 : (entries_.size() + kPerPage - 1) / kPerPage;
}

void LauncherUi::cellCenter(size_t slot, int16_t &cx, int16_t &cy) const
{
    cx = (int16_t)(kCellW / 2 + (slot % kCols) * kCellW);
    cy = kRowCenterY[slot / kCols];
}

int LauncherUi::entryAt(int16_t x, int16_t y) const
{
    for (size_t slot = 0; slot < kPerPage; ++slot) {
        const size_t index = page_ * kPerPage + slot;
        if (index >= entries_.size()) {
            break;
        }
        int16_t cx, cy;
        cellCenter(slot, cx, cy);
        const Rect cell = {int16_t(cx - kCellW / 2), int16_t(cy - kCellHalfH), kCellW, int16_t(2 * kCellHalfH)};
        if (cell.contains(x, y)) {
            return (int)index;
        }
    }
    return -1;
}

void LauncherUi::show(const std::vector<AppTile> &tiles)
{
    tiles_ = tiles;
    entries_ = {
        {Icon::Settings, LauncherAction::Settings, "Cai dat", -1},
        {Icon::Face, LauncherAction::Face, "Claude", -1},
        {Icon::Chat, LauncherAction::Chat, "Chat", -1},
        {Icon::Games, LauncherAction::Games, "Tro choi", -1},
        {Icon::Map, LauncherAction::Map, "Ban do", -1},
        {Icon::NewApp, LauncherAction::NewApp, "+ App", -1},
    };
    for (size_t i = 0; i < tiles_.size(); ++i) {
        entries_.push_back({ui_icons::iconForApp(tiles_[i].id), LauncherAction::OpenApp, nullptr, (int)i});
    }
    if (page_ >= pageCount()) {
        page_ = pageCount() - 1;
    }
    visible_ = true;
    menuVisible_ = false;
    draw();
}

void LauncherUi::draw()
{
    gfx_.fillScreen(kColorBlack);
    drawTopLine();
    for (size_t slot = 0; slot < kPerPage; ++slot) {
        const size_t index = page_ * kPerPage + slot;
        if (index >= entries_.size()) {
            break;
        }
        const Entry &e = entries_[index];
        int16_t cx, cy;
        cellCenter(slot, cx, cy);
        if (e.appIndex >= 0) {
            const AppTile &tile = tiles_[(size_t)e.appIndex];
            ui_icons::drawWithLabel(gfx_, e.icon, cx, cy, kCellW, tile.name, tile.color);
        } else {
            ui_icons::drawWithLabel(gfx_, e.icon, cx, cy, kCellW, e.label);
        }
    }
    drawNotice();
    drawPager();
}

void LauncherUi::drawTopLine()
{
    gfx_.fillRect(0, 0, kScreenW, kTopLineH, kColorBlack);
    gfx_.setTextWrap(false);
    gfx_.setTextSize(2);
    gfx_.setTextColor(kColorWhite);
    gfx_.setCursor(8, 5);
    gfx_.print(clock_.length() > 0 ? clock_ : String("--:--"));
    if (status_.length() > 0) {
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorMuted);
        gfx_.setCursor(kScreenW - (int16_t)status_.length() * kFontW - 8, 9);
        gfx_.print(status_);
    }
}

void LauncherUi::drawNotice()
{
    gfx_.fillRect(kArrowW, kNoticeY, kScreenW - 2 * kArrowW, kFontH + 2, kColorBlack);
    const String text = notice_.length() > 0 ? notice_ : String("Cham de mo - giu app 1s de sua/xoa");
    drawCentered(gfx_, ellipsize(text, (kScreenW - 2 * kArrowW) / kFontW), kArrowW, kScreenW - 2 * kArrowW,
                 kNoticeY, 1, kColorMuted);
}

void LauncherUi::drawPager()
{
    const size_t pages = pageCount();
    gfx_.fillRect(kArrowW, kPagerY - kPagerDotR - 1, kScreenW - 2 * kArrowW, 2 * kPagerDotR + 3, kColorBlack);
    const int16_t startX = kScreenW / 2 - (int16_t)(pages - 1) * kPagerDotGap / 2;
    for (size_t i = 0; i < pages; ++i) {
        const int16_t x = startX + (int16_t)i * kPagerDotGap;
        if (i == page_) {
            gfx_.fillCircle(x, kPagerY, kPagerDotR, kColorAccent);
        } else {
            gfx_.drawCircle(x, kPagerY, kPagerDotR, kColorMuted);
        }
    }
    const uint16_t prev = page_ > 0 ? kColorWhite : kColorDivider;
    const uint16_t next = page_ + 1 < pages ? kColorWhite : kColorDivider;
    gfx_.fillRect(prevButton_.x, kPagerY - kArrowSize - 1, kArrowW, 2 * kArrowSize + 3, kColorBlack);
    gfx_.fillRect(nextButton_.x, kPagerY - kArrowSize - 1, kArrowW, 2 * kArrowSize + 3, kColorBlack);
    drawChevronLeft(gfx_, 16, kPagerY - kArrowSize, kArrowSize, prev);
    drawChevronRight(gfx_, kScreenW - 16 - kArrowSize - 2, kPagerY - kArrowSize, kArrowSize, next);
}

void LauncherUi::goToPage(size_t page)
{
    if (page < pageCount() && page != page_) {
        page_ = page;
        draw();
    }
}

void LauncherUi::setStatus(const String &clock, const String &status)
{
    if (clock == clock_ && status == status_) {
        return;
    }
    clock_ = clock;
    status_ = status;
    if (visible_ && !menuVisible_) {
        drawTopLine();
    }
}

void LauncherUi::setNotice(const String &notice)
{
    notice_ = notice;
    if (visible_ && !menuVisible_) {
        drawNotice();
    }
}

LauncherAction LauncherUi::handleTap(int16_t x, int16_t y)
{
    if (prevButton_.contains(x, y)) {
        if (page_ > 0) {
            goToPage(page_ - 1);
        }
        return LauncherAction::None;
    }
    if (nextButton_.contains(x, y)) {
        goToPage(page_ + 1);
        return LauncherAction::None;
    }
    const int index = entryAt(x, y);
    if (index < 0) {
        return LauncherAction::None;
    }
    const Entry &e = entries_[(size_t)index];
    if (e.appIndex >= 0) {
        selected_ = (size_t)e.appIndex;
    }
    return e.action;
}

LauncherAction LauncherUi::handleLongPress(int16_t x, int16_t y)
{
    const int index = entryAt(x, y);
    if (index < 0 || entries_[(size_t)index].appIndex < 0) {
        return LauncherAction::None;
    }
    selected_ = (size_t)entries_[(size_t)index].appIndex;
    return LauncherAction::AppMenu;
}

void LauncherUi::handleSwipe(int16_t dx)
{
    if (dx <= -kSwipeMin) {
        goToPage(page_ + 1);
    } else if (dx >= kSwipeMin && page_ > 0) {
        goToPage(page_ - 1);
    }
}

void LauncherUi::showMenu(const AppTile &tile)
{
    menuVisible_ = true;
    gfx_.fillRoundRect(kMenuX - 2, kMenuY - 2, kMenuW + 4, kMenuH + 4, kMenuRadius, kColorMuted);
    gfx_.fillRoundRect(kMenuX, kMenuY, kMenuW, kMenuH, kMenuRadius, kColorHeader);
    drawCentered(gfx_, ellipsize(tile.name, (kMenuW - 12) / (kFontW * 2)), kMenuX, kMenuW, kMenuY + 12, 2,
                 kColorAccent);
    drawCentered(gfx_, "Sua: mo ta thay doi, Claude viet lai", kMenuX, kMenuW, kMenuY + 36, 1, kColorMuted);
    drawButton(gfx_, menuButtons_[0], "Chay", kColorOk, kColorWhite);
    drawButton(gfx_, menuButtons_[1], "Sua", kColorAccent, kColorWhite);
    drawButton(gfx_, menuButtons_[2], "Xoa", kColorDanger, kColorWhite);
    drawButton(gfx_, menuButtons_[3], "Huy", kColorButton, kColorWhite);
}

MenuAction LauncherUi::handleMenuTap(int16_t x, int16_t y)
{
    static const MenuAction kActions[kMenuButtons] = {MenuAction::Run, MenuAction::Edit, MenuAction::Delete,
                                                      MenuAction::Cancel};
    for (size_t i = 0; i < kMenuButtons; ++i) {
        if (menuButtons_[i].contains(x, y)) {
            menuVisible_ = false;
            return kActions[i];
        }
    }
    return MenuAction::None;
}
