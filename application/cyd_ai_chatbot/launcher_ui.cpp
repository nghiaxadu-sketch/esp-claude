#include "launcher_ui.h"

#include <string>

#include "text_utils.h"

namespace {

using namespace ui;

constexpr const char *kTitle = "Claude Apps";
constexpr int16_t kGridTop = kHeaderH + 4;
constexpr int16_t kNoticeH = 12;
constexpr int16_t kGridBottom = kScreenH - kFooterH - kNoticeH - 2;
constexpr int16_t kTileGap = 6;
constexpr int16_t kTileRadius = 8;
constexpr uint8_t kTileTextSize = 2;
constexpr size_t kTileMaxLines = 3;

constexpr int16_t kMenuX = 30;
constexpr int16_t kMenuY = 40;
constexpr int16_t kMenuW = kScreenW - 2 * kMenuX;
constexpr int16_t kMenuH = 160;
constexpr int16_t kMenuButtonH = 40;

// Picks black or white text for a tile color.
uint16_t textColorOn(uint16_t bg)
{
    const uint16_t r = (bg >> 11) & 0x1F, g = (bg >> 5) & 0x3F, b = bg & 0x1F;
    const uint32_t luma = r * 2 * 299 + g * 587 + b * 2 * 114;  // channels scaled to 0..63
    return luma > 32 * 1000 ? 0x0000 : kColorWhite;
}

}  // namespace

LauncherUi::LauncherUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
    for (size_t i = 0; i < kFooterButtons; ++i) {
        footer_[i] = footerSlot(i, kFooterButtons);
    }
    const int16_t bw = (kMenuW - 3 * kTileGap) / 2;
    const int16_t row1 = kMenuY + kMenuH - 2 * kMenuButtonH - 2 * kTileGap;
    const int16_t row2 = row1 + kMenuButtonH + kTileGap;
    menuButtons_[0] = {int16_t(kMenuX + kTileGap), row1, bw, kMenuButtonH};               // Run
    menuButtons_[1] = {int16_t(kMenuX + 2 * kTileGap + bw), row1, bw, kMenuButtonH};      // Edit
    menuButtons_[2] = {int16_t(kMenuX + kTileGap), row2, bw, kMenuButtonH};               // Delete
    menuButtons_[3] = {int16_t(kMenuX + 2 * kTileGap + bw), row2, bw, kMenuButtonH};      // Cancel
}

size_t LauncherUi::pageCount() const
{
    return tiles_.empty() ? 1 : (tiles_.size() + kPerPage - 1) / kPerPage;
}

Rect LauncherUi::tileRect(size_t slot) const
{
    const int16_t w = (kScreenW - (kCols + 1) * kTileGap) / kCols;
    const int16_t h = (kGridBottom - kGridTop - (kRows - 1) * kTileGap) / kRows;
    const size_t col = slot % kCols, row = slot / kCols;
    return {int16_t(kTileGap + col * (w + kTileGap)), int16_t(kGridTop + row * (h + kTileGap)), w, h};
}

int LauncherUi::tileAt(int16_t x, int16_t y) const
{
    for (size_t slot = 0; slot < kPerPage; ++slot) {
        const size_t index = page_ * kPerPage + slot;
        if (index < tiles_.size() && tileRect(slot).contains(x, y)) {
            return (int)index;
        }
    }
    return -1;
}

void LauncherUi::show(const std::vector<AppTile> &tiles)
{
    tiles_ = tiles;
    if (page_ >= pageCount()) {
        page_ = pageCount() - 1;
    }
    visible_ = true;
    menuVisible_ = false;
    draw();
}

void LauncherUi::draw()
{
    gfx_.fillScreen(kColorBg);
    gfx_.setTextWrap(false);
    drawHeader(gfx_, kTitle, status_);

    if (tiles_.empty()) {
        drawSplash(gfx_, "Chua co app", "Bam \"+ App\" roi mo ta app ban muon,", "Claude se viet va chay ngay.");
        drawHeader(gfx_, kTitle, status_);
    }
    for (size_t slot = 0; slot < kPerPage; ++slot) {
        const size_t index = page_ * kPerPage + slot;
        if (index >= tiles_.size()) {
            break;
        }
        const AppTile &tile = tiles_[index];
        const Rect r = tileRect(slot);
        gfx_.fillRoundRect(r.x, r.y, r.w, r.h, kTileRadius, tile.color);
        const size_t perLine = (size_t)(r.w - 8) / (kFontW * kTileTextSize);
        std::vector<std::string> lines = text_utils::wrapText(tile.name.c_str(), perLine);
        if (lines.size() > kTileMaxLines) {
            lines.resize(kTileMaxLines);
        }
        const int16_t lineH = kFontH * kTileTextSize + 4;
        int16_t y = r.y + (r.h - (int16_t)lines.size() * lineH) / 2 + 2;
        gfx_.setTextSize(kTileTextSize);
        gfx_.setTextColor(textColorOn(tile.color));
        for (const std::string &line : lines) {
            gfx_.setCursor(r.x + (r.w - (int16_t)line.size() * kFontW * kTileTextSize) / 2, y);
            gfx_.print(line.c_str());
            y += lineH;
        }
    }

    const String pageLabel = pageCount() > 1 ? String(page_ + 1) + "/" + String(pageCount()) : String("Trang");
    drawButton(gfx_, footer_[0], "Chat", kColorButton, kColorWhite);
    drawButton(gfx_, footer_[1], "+ App", kColorAccent, kColorWhite);
    drawButton(gfx_, footer_[2], "Wi-Fi", kColorButton, kColorWhite);
    drawButton(gfx_, footer_[3], pageLabel.c_str(), kColorButton, pageCount() > 1 ? kColorWhite : kColorMuted);
    drawNotice();
}

void LauncherUi::drawNotice()
{
    const int16_t y = kScreenH - kFooterH - kNoticeH;
    gfx_.fillRect(0, y, kScreenW, kNoticeH, kColorBg);
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorMuted);
    const String text = notice_.length() > 0 ? notice_ : String("Cham de mo, giu 1 giay de sua/xoa app");
    gfx_.setCursor(kMargin, y + 2);
    gfx_.print(ellipsize(text, (kScreenW - 2 * kMargin) / kFontW));
}

void LauncherUi::setStatus(const String &status)
{
    if (status == status_) {
        return;
    }
    status_ = status;
    if (visible_ && !menuVisible_) {
        drawHeader(gfx_, kTitle, status_);
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
    if (footer_[0].contains(x, y)) {
        return LauncherAction::Chat;
    }
    if (footer_[1].contains(x, y)) {
        return LauncherAction::NewApp;
    }
    if (footer_[2].contains(x, y)) {
        return LauncherAction::Wifi;
    }
    if (footer_[3].contains(x, y)) {
        if (pageCount() > 1) {
            page_ = (page_ + 1) % pageCount();
            draw();
        }
        return LauncherAction::None;
    }
    const int index = tileAt(x, y);
    if (index < 0) {
        return LauncherAction::None;
    }
    selected_ = (size_t)index;
    return LauncherAction::OpenApp;
}

LauncherAction LauncherUi::handleLongPress(int16_t x, int16_t y)
{
    const int index = tileAt(x, y);
    if (index < 0) {
        return LauncherAction::None;
    }
    selected_ = (size_t)index;
    return LauncherAction::AppMenu;
}

void LauncherUi::showMenu(const AppTile &tile)
{
    menuVisible_ = true;
    gfx_.fillRoundRect(kMenuX - 2, kMenuY - 2, kMenuW + 4, kMenuH + 4, kTileRadius, kColorMuted);
    gfx_.fillRoundRect(kMenuX, kMenuY, kMenuW, kMenuH, kTileRadius, kColorHeader);
    gfx_.setTextSize(2);
    gfx_.setTextColor(kColorAccent);
    const String name = ellipsize(tile.name, (kMenuW - 12) / (kFontW * 2));
    gfx_.setCursor(kMenuX + (kMenuW - (int16_t)name.length() * kFontW * 2) / 2, kMenuY + 12);
    gfx_.print(name);
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(kMenuX + 8, kMenuY + 36);
    gfx_.print("Sua: mo ta thay doi, Claude viet lai");
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
