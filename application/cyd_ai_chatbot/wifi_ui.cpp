#include "wifi_ui.h"

namespace {

using namespace ui;

constexpr const char *kTitle = "Wi-Fi";
constexpr int16_t kStatusY = kHeaderH + 2;
constexpr int16_t kStatusH = 14;
constexpr int16_t kListTop = kStatusY + kStatusH + 2;
constexpr int16_t kListBottom = kScreenH - kFooterH - 2;
constexpr int16_t kRowH = 30;
constexpr int16_t kBarW = 4;
constexpr int16_t kBarGap = 2;
constexpr int16_t kBarsWidth = 4 * (kBarW + kBarGap);
constexpr int16_t kBarMaxH = 16;
constexpr uint8_t kNameTextSize = 2;

}  // namespace

WifiUi::WifiUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
    for (size_t i = 0; i < kFooterButtons; ++i) {
        footer_[i] = footerSlot(i, kFooterButtons);
    }
}

int WifiUi::visibleRows() const
{
    return (kListBottom - kListTop) / kRowH;
}

void WifiUi::showScanning()
{
    visible_ = true;
    drawSplash(gfx_, "Dang quet Wi-Fi...", "Doi vai giay");
}

void WifiUi::show(const std::vector<WifiNetwork> &networks, const String &status)
{
    networks_ = networks;
    status_ = status;
    scrollTop_ = 0;
    visible_ = true;
    gfx_.fillScreen(kColorBg);
    gfx_.setTextWrap(false);
    drawHeader(gfx_, kTitle, String(networks_.size()) + " mang");
    drawStatus();
    drawList();
    drawButton(gfx_, footer_[0], "Ve", kColorButton, kColorWhite);
    drawButton(gfx_, footer_[1], "Len", kColorButton, kColorWhite);
    drawButton(gfx_, footer_[2], "Xuong", kColorButton, kColorWhite);
    drawButton(gfx_, footer_[3], "Quet", kColorAccent, kColorWhite);
}

void WifiUi::setStatus(const String &status)
{
    status_ = status;
    if (visible_) {
        drawStatus();
    }
}

void WifiUi::drawStatus()
{
    gfx_.fillRect(0, kStatusY, kScreenW, kStatusH, kColorBg);
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorSystem);
    gfx_.setCursor(kMargin, kStatusY + 3);
    gfx_.print(ellipsize(status_, (kScreenW - 2 * kMargin) / kFontW));
}

void WifiUi::drawBars(int16_t x, int16_t y, uint8_t bars)
{
    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t h = kBarMaxH * (i + 1) / 4;
        gfx_.fillRect(x + i * (kBarW + kBarGap), y + kBarMaxH - h, kBarW, h, i < bars ? kColorOk : kColorButton);
    }
}

void WifiUi::drawList()
{
    gfx_.fillRect(0, kListTop, kScreenW, kListBottom - kListTop, kColorBg);
    if (networks_.empty()) {
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorMuted);
        gfx_.setCursor(kMargin, kListTop + 10);
        gfx_.print("Khong thay mang nao. Bam \"Quet\" de thu lai.");
        return;
    }
    const String current = wifi_manager::connected() ? wifi_manager::ssid() : String();
    const size_t nameChars = (kScreenW - 3 * kMargin - kBarsWidth - 2 * kFontW) / (kFontW * kNameTextSize);
    for (int row = 0; row < visibleRows(); ++row) {
        const size_t index = scrollTop_ + row;
        if (index >= networks_.size()) {
            break;
        }
        const WifiNetwork &n = networks_[index];
        const int16_t y = kListTop + row * kRowH;
        const bool isCurrent = current.length() > 0 && n.ssid == current;
        gfx_.fillRoundRect(kMargin, y + 1, kScreenW - 2 * kMargin, kRowH - 2, 4, isCurrent ? kColorOk : kColorHeader);
        gfx_.setTextSize(kNameTextSize);
        gfx_.setTextColor(kColorWhite);
        gfx_.setCursor(kMargin + 4, y + (kRowH - kFontH * kNameTextSize) / 2);
        gfx_.print(ellipsize(toAscii(n.ssid), nameChars));
        gfx_.setTextSize(1);
        gfx_.setTextColor(n.open ? kColorSystem : kColorMuted);
        gfx_.setCursor(kScreenW - kMargin - kBarsWidth - 2 * kFontW - 4, y + (kRowH - kFontH) / 2);
        gfx_.print(n.open ? "  " : "**");  // "**" = needs a password
        drawBars(kScreenW - kMargin - kBarsWidth, y + (kRowH - kBarMaxH) / 2, wifi_manager::bars(n.rssi));
    }
}

WifiAction WifiUi::handleTouch(int16_t x, int16_t y)
{
    if (footer_[0].contains(x, y)) {
        return WifiAction::Back;
    }
    if (footer_[1].contains(x, y)) {
        if (scrollTop_ > 0) {
            scrollTop_ = scrollTop_ > (size_t)visibleRows() ? scrollTop_ - visibleRows() : 0;
            drawList();
        }
        return WifiAction::None;
    }
    if (footer_[2].contains(x, y)) {
        if (scrollTop_ + visibleRows() < networks_.size()) {
            scrollTop_ += visibleRows();
            drawList();
        }
        return WifiAction::None;
    }
    if (footer_[3].contains(x, y)) {
        return WifiAction::Rescan;
    }
    if (y >= kListTop && y < kListBottom) {
        const size_t index = scrollTop_ + (y - kListTop) / kRowH;
        if (index < networks_.size()) {
            selected_ = index;
            return WifiAction::Select;
        }
    }
    return WifiAction::None;
}
