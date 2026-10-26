#include "settings_ui.h"

namespace {

using namespace ui;

constexpr int16_t kRowH = 36;
constexpr int16_t kRowIconX = 20;
constexpr int16_t kRowLabelX = 40;
constexpr int16_t kRowChevronX = 302;
constexpr int16_t kRowChevronSize = 5;
constexpr int16_t kRowValueRight = 294;

// AI model page.
constexpr int16_t kInfoX = 8;
constexpr int16_t kModelRowTop = 58;
constexpr int16_t kModelRowBottom = 92;
constexpr int16_t kQrX = 222;
constexpr int16_t kQrY = 126;
constexpr int16_t kQrMaxSide = 92;
constexpr int16_t kQrQuietModules = 2;

// Display page.
constexpr int16_t kSliderX = 20;
constexpr int16_t kSliderY = 62;
constexpr int16_t kSliderH = 46;
constexpr int16_t kSegments = 10;
constexpr int16_t kSegmentGap = 6;
constexpr int16_t kSegmentW = (kScreenW - 2 * kSliderX - (kSegments - 1) * kSegmentGap) / kSegments;
constexpr int16_t kSliderW = kSegments * kSegmentW + (kSegments - 1) * kSegmentGap;
constexpr int16_t kSliderHitPad = 10;
constexpr uint8_t kBrightnessStep = 5;
constexpr uint8_t kBrightnessMin = 5;
constexpr Rect kRotateButton = {206, 130, 106, 34};
constexpr Rect kScreenOffButton = {206, 176, 106, 34};
constexpr uint16_t kScreenOffChoices[] = {30, 60, 120, 300, 600, 0};

const char *const kTitle = "Cai dat";

// ---------------------------------------------------------------- small row glyphs

void glyphAi(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.fillRoundRect(cx - 9, cy - 7, 18, 14, 4, kColorAccent);
    g.fillRect(cx - 5, cy - 3, 3, 5, kColorBlack);
    g.fillRect(cx + 2, cy - 3, 3, 5, kColorBlack);
    g.drawFastVLine(cx, cy - 11, 4, kColorAccent);
}

void glyphWifi(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    for (int16_t r = 4; r <= 12; r += 4) {
        g.drawCircleHelper(cx, cy + 6, r, 0x1 | 0x2, kColorAccent);
        g.drawCircleHelper(cx, cy + 6, r - 1, 0x1 | 0x2, kColorAccent);
    }
    g.fillCircle(cx, cy + 6, 2, kColorAccent);
}

void glyphSun(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.fillCircle(cx, cy, 5, kColorAccent);
    static const int8_t kRays[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
    for (const auto &r : kRays) {
        g.drawLine(cx + r[0] * 8, cy + r[1] * 8, cx + r[0] * 10, cy + r[1] * 10, kColorAccent);
    }
}

void glyphSpeaker(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.fillRect(cx - 9, cy - 3, 5, 7, kColorAccent);
    g.fillTriangle(cx - 5, cy, cx + 1, cy - 7, cx + 1, cy + 7, kColorAccent);
    g.drawCircleHelper(cx + 2, cy, 5, 0x2 | 0x4, kColorAccent);
    g.drawCircleHelper(cx + 2, cy, 9, 0x2 | 0x4, kColorAccent);
}

void glyphPen(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    for (int16_t t = -1; t <= 1; ++t) {
        g.drawLine(cx - 6 + t, cy + 6, cx + 8 + t, cy - 8, kColorAccent);
    }
    g.fillTriangle(cx - 10, cy + 10, cx - 8, cy + 4, cx - 4, cy + 8, kColorAccent);
}

void glyphChip(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.drawRect(cx - 7, cy - 7, 14, 14, kColorAccent);
    g.fillRect(cx - 3, cy - 3, 6, 6, kColorAccent);
    for (int16_t i = -4; i <= 4; i += 4) {
        g.drawFastHLine(cx - 10, cy + i, 3, kColorAccent);
        g.drawFastHLine(cx + 8, cy + i, 3, kColorAccent);
    }
}

using Glyph = void (*)(Adafruit_GFX &, int16_t, int16_t);
const Glyph kGlyphs[] = {glyphAi, glyphWifi, glyphSun, glyphSpeaker, glyphPen, glyphChip};

String uptimeText(uint32_t sec)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%luh %02lum", (unsigned long)(sec / 3600), (unsigned long)(sec / 60 % 60));
    return String(buf);
}

}  // namespace

SettingsUi::SettingsUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
}

void SettingsUi::show(const SettingsInfo &info, const DeviceSettings &device)
{
    info_ = info;
    device_ = device;
    page_ = Page::List;
    visible_ = true;
    draw();
}

void SettingsUi::update(const SettingsInfo &info)
{
    info_ = info;
    if (visible_) {
        draw();
    }
}

void SettingsUi::draw()
{
    gfx_.fillScreen(kColorBlack);
    switch (page_) {
    case Page::List: drawList(); break;
    case Page::Model: drawModel(); break;
    case Page::Display: drawDisplay(); break;
    case Page::About: drawAbout(); break;
    }
}

// ---------------------------------------------------------------- list

void SettingsUi::drawRow(size_t row, const char *label, const String &value, uint16_t valueColor)
{
    const int16_t top = kHeaderH + (int16_t)row * kRowH;
    const int16_t cy = top + kRowH / 2;
    gfx_.fillRect(0, top, kScreenW, kRowH, kColorBlack);
    kGlyphs[row](gfx_, kRowIconX, cy);
    gfx_.setTextSize(2);
    gfx_.setTextColor(kColorText);
    gfx_.setCursor(kRowLabelX, cy - kFontH + 1);
    gfx_.print(label);
    const String v = ellipsize(value, 18);
    gfx_.setTextSize(1);
    gfx_.setTextColor(valueColor);
    gfx_.setCursor(kRowValueRight - (int16_t)v.length() * kFontW, cy - kFontH / 2);
    gfx_.print(v);
    drawChevronRight(gfx_, kRowChevronX, cy - kRowChevronSize, kRowChevronSize, kColorMuted);
    if (row + 1 < kRows) {
        gfx_.drawFastHLine(kRowLabelX, top + kRowH - 1, kScreenW - kRowLabelX - 8, kColorDivider);
    }
}

void SettingsUi::drawList()
{
    drawBackHeader(gfx_, kTitle);
    gfx_.fillRect(kScreenW - 18, 6, 10, 10, kColorAccent);  // the orange square of the reference design
    drawRow(0, "AI Model", info_.hasApiKey ? String("Da lien ket") : String("Chua co key"),
            info_.hasApiKey ? kColorText : kColorSystem);
    drawRow(1, "Wi-Fi", info_.wifiConnected ? info_.ssid : String("Chua ket noi"),
            info_.wifiConnected ? kColorText : kColorSystem);
    drawRow(2, "Man hinh", String(device_.brightness) + "%", kColorText);
    drawRow(3, "Am thanh", !info_.hasSpeaker ? String("Khong co loa") : (device_.sound ? "Bat" : "Tat"),
            info_.hasSpeaker ? kColorText : kColorMuted);
    drawRow(4, "But cam ung", "Hieu chinh", kColorText);
    drawRow(5, "Thiet bi", info_.board, kColorText);
}

// ---------------------------------------------------------------- AI model

void SettingsUi::drawInfoLine(int16_t y, const char *label, const String &value, uint16_t color)
{
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(kInfoX, y);
    gfx_.print(label);
    gfx_.setTextSize(2);
    gfx_.setTextColor(color);
    gfx_.setCursor(kInfoX, y + 10);
    gfx_.print(ellipsize(value, 17));
}

void SettingsUi::drawQr(const String &text, int16_t x, int16_t y, int16_t maxSide)
{
    if (text != qrText_) {
        qrText_ = text;
        if (!qr_code::encode(text.c_str(), qr_)) {
            qr_.size = 0;
        }
    }
    if (qr_.size == 0) {
        return;
    }
    const int16_t modules = (int16_t)qr_.size + 2 * kQrQuietModules;
    const int16_t scale = max<int16_t>(1, maxSide / modules);
    const int16_t side = modules * scale;
    gfx_.fillRect(x, y, side, side, kColorWhite);
    const int16_t ox = x + kQrQuietModules * scale, oy = y + kQrQuietModules * scale;
    for (int16_t my = 0; my < qr_.size; ++my) {
        for (int16_t mx = 0; mx < qr_.size; ++mx) {
            if (qr_.dark(mx, my)) {
                gfx_.fillRect(ox + mx * scale, oy + my * scale, scale, scale, kColorBlack);
            }
        }
    }
}

void SettingsUi::drawModel()
{
    drawBackHeader(gfx_, "AI Model", info_.hasApiKey ? "Da lien ket" : "Chua co key",
                   info_.hasApiKey ? kColorOk : kColorSystem);
    drawInfoLine(26, "Backend", "Anthropic Claude");
    drawInfoLine(kModelRowTop, "Model (cham de doi)", info_.model, kColorUser);
    drawInfoLine(92, "API key", info_.hasApiKey ? "Da luu" : "Chua co - nhap tren web",
                 info_.hasApiKey ? kColorText : kColorSystem);
    drawInfoLine(126, "Kha nang", "Chat - App - Mat");
    drawInfoLine(160, "Base URL", "api.anthropic.com");

    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(kInfoX, 204);
    if (info_.wifiConnected) {
        gfx_.print("STA: " + ellipsize(info_.ssid, 30));
        gfx_.setTextColor(kColorWhite);
        gfx_.setCursor(kInfoX, 218);
        gfx_.print("http://" + info_.ip + "/");
        drawQr("http://" + info_.ip + "/", kQrX, kQrY, kQrMaxSide);
    } else {
        gfx_.print("Chua co Wi-Fi: noi mang de mo");
        gfx_.setCursor(kInfoX, 218);
        gfx_.print("trang cai dat tren dien thoai.");
    }
}

// ---------------------------------------------------------------- display

void SettingsUi::drawBrightness()
{
    gfx_.fillRect(0, 26, kScreenW, kSliderY + kSliderH + 20 - 26, kColorBlack);
    gfx_.setTextSize(2);
    gfx_.setTextColor(kColorText);
    gfx_.setCursor(10, 34);
    gfx_.print("Do sang");
    const String value = String(device_.brightness) + "%";
    gfx_.setTextSize(3);
    gfx_.setTextColor(kColorAccent);
    gfx_.setCursor(kScreenW - 10 - (int16_t)value.length() * kFontW * 3, 30);
    gfx_.print(value);
    const int16_t lit = (int16_t)((device_.brightness + kSegments - 1) / kSegments);  // 1..10
    for (int16_t i = 0; i < kSegments; ++i) {
        const int16_t x = kSliderX + i * (kSegmentW + kSegmentGap);
        gfx_.fillRoundRect(x, kSliderY, kSegmentW, kSliderH, 4, i < lit ? kColorAccent : kColorDivider);
    }
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(kSliderX, kSliderY + kSliderH + 6);
    gfx_.print("0");
    gfx_.setCursor(kSliderX + kSliderW / 2 - 6, kSliderY + kSliderH + 6);
    gfx_.print("50");
    gfx_.setCursor(kSliderX + kSliderW - 18, kSliderY + kSliderH + 6);
    gfx_.print("100");
}

String SettingsUi::screenOffText() const
{
    if (device_.screenOffSec == 0) {
        return "Khong tat";
    }
    return device_.screenOffSec < 60 ? String(device_.screenOffSec) + " giay"
                                     : String(device_.screenOffSec / 60) + " phut";
}

void SettingsUi::drawDisplay()
{
    drawBackHeader(gfx_, "Man hinh");
    drawBrightness();
    gfx_.drawFastHLine(10, 124, kScreenW - 20, kColorDivider);
    gfx_.setTextSize(2);
    gfx_.setTextColor(kColorText);
    gfx_.setCursor(10, kRotateButton.y + 9);
    gfx_.print("Xoay man");
    drawButton(gfx_, kRotateButton, device_.flipped ? "180 do" : "0 do", kColorButton, kColorWhite);
    gfx_.setCursor(10, kScreenOffButton.y + 9);
    gfx_.setTextColor(kColorText);
    gfx_.setTextSize(2);
    gfx_.print("Tat sau");
    drawButton(gfx_, kScreenOffButton, screenOffText().c_str(), kColorButton, kColorWhite);
    drawCentered(gfx_, "Keo but tren thanh de chinh do sang", 0, kScreenW, 224, 1, kColorMuted);
}

SettingsAction SettingsUi::setBrightnessFrom(int16_t x)
{
    const int32_t raw = (int32_t)(x - kSliderX) * 100 / kSliderW;
    int32_t percent = (raw + kBrightnessStep / 2) / kBrightnessStep * kBrightnessStep;
    percent = percent < kBrightnessMin ? kBrightnessMin : (percent > 100 ? 100 : percent);
    if (percent == device_.brightness) {
        return SettingsAction::None;
    }
    device_.brightness = (uint8_t)percent;
    drawBrightness();
    return SettingsAction::DeviceChanged;
}

// ---------------------------------------------------------------- about

void SettingsUi::drawAbout()
{
    drawBackHeader(gfx_, "Thiet bi");
    int16_t y = 30;
    auto line = [&](const char *label, const String &value) {
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorMuted);
        gfx_.setCursor(10, y);
        gfx_.print(label);
        gfx_.setTextColor(kColorText);
        gfx_.setCursor(110, y);
        gfx_.print(ellipsize(value, 34));
        y += 18;
    };
    line("Board", info_.board);
    line("Phan mem", "Claude Apps 2.0");
    line("Wi-Fi", info_.wifiConnected ? info_.ssid + "  " + info_.ip : String("chua ket noi"));
    line("Model", info_.model);
    line("App da luu", String((unsigned)info_.appCount));
    line("RAM trong", String(info_.freeHeap / 1024) + " KB");
    line("PSRAM trong", info_.freePsram > 0 ? String(info_.freePsram / 1024) + " KB" : String("khong co"));
    line("Da chay", uptimeText(info_.uptimeSec));
    line("Loa", info_.hasSpeaker ? "co" : "khong");
    drawCentered(gfx_, "Lenh Serial: /status /help", 0, kScreenW, 216, 1, kColorMuted);
}

// ---------------------------------------------------------------- input

SettingsAction SettingsUi::handleTouch(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        if (page_ == Page::List) {
            return SettingsAction::Back;
        }
        page_ = Page::List;
        draw();
        return SettingsAction::None;
    }
    switch (page_) {
    case Page::List: {
        if (y < kHeaderH) {
            return SettingsAction::None;
        }
        const int row = (y - kHeaderH) / kRowH;
        switch (row) {
        case 0: page_ = Page::Model; draw(); break;
        case 1: return SettingsAction::Wifi;
        case 2: page_ = Page::Display; draw(); break;
        case 3:
            if (info_.hasSpeaker) {
                device_.sound = !device_.sound;
                drawRow(3, "Am thanh", device_.sound ? "Bat" : "Tat", kColorText);
                return SettingsAction::DeviceChanged;
            }
            break;
        case 4: return SettingsAction::Calibrate;
        case 5: page_ = Page::About; draw(); break;
        default: break;
        }
        return SettingsAction::None;
    }
    case Page::Model:
        return y >= kModelRowTop && y < kModelRowBottom ? SettingsAction::NextModel : SettingsAction::None;
    case Page::Display:
        if (kRotateButton.contains(x, y)) {
            device_.flipped = !device_.flipped;
            return SettingsAction::DeviceChanged;  // the caller turns the screen and calls update()
        }
        if (kScreenOffButton.contains(x, y)) {
            size_t i = 0;
            const size_t count = sizeof(kScreenOffChoices) / sizeof(kScreenOffChoices[0]);
            while (i < count && kScreenOffChoices[i] != device_.screenOffSec) {
                ++i;
            }
            device_.screenOffSec = kScreenOffChoices[(i + 1) % count];
            drawButton(gfx_, kScreenOffButton, screenOffText().c_str(), kColorButton, kColorWhite);
            return SettingsAction::DeviceChanged;
        }
        return handleDrag(x, y);
    case Page::About:
        break;
    }
    return SettingsAction::None;
}

SettingsAction SettingsUi::handleDrag(int16_t x, int16_t y)
{
    if (page_ != Page::Display || y < kSliderY - kSliderHitPad || y > kSliderY + kSliderH + kSliderHitPad) {
        return SettingsAction::None;
    }
    return setBrightnessFrom(x);
}
