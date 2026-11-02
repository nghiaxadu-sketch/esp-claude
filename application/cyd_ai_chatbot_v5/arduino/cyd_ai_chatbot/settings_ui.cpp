#include "settings_ui.h"

#include "firmware_version.h"
#include "settings_ui_internal.h"

using namespace settings_layout;

namespace {

using namespace ui;

// AI model page.
constexpr int16_t kInfoX = 8;
constexpr int16_t kModelRowTop = 58;
constexpr int16_t kModelRowBottom = 92;
constexpr int16_t kQrX = 222;
constexpr int16_t kQrY = 126;
constexpr int16_t kQrMaxSide = 92;
constexpr int16_t kQrQuietModules = 2;

// Display page.
constexpr uint8_t kBrightnessStep = 5;
constexpr uint8_t kBrightnessMin = 5;
constexpr Rect kRotateButton = {8, 128, 150, 36};
constexpr Rect kScreenOffButton = {162, 128, 150, 36};
constexpr Rect kThemeButton = {8, 172, 150, 36};
constexpr Rect kAccentButton = {162, 172, 150, 36};
constexpr uint16_t kScreenOffChoices[] = {30, 60, 120, 300, 600, 0};
constexpr Rect kListPageHit = {250, 0, 70, 26};

const char *const kTitle = "Cai dat";

// ---------------------------------------------------------------- small row glyphs

void glyphAi(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.fillRoundRect(cx - 9, cy - 7, 18, 14, 4, kColorAccent);
    g.fillRect(cx - 5, cy - 3, 3, 5, kColorBg);
    g.fillRect(cx + 2, cy - 3, 3, 5, kColorBg);
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

void glyphRole(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.fillRoundRect(cx - 10, cy - 8, 20, 12, 5, kColorAccent);
    g.fillTriangle(cx - 4, cy + 3, cx + 2, cy + 3, cx - 6, cy + 9, kColorAccent);
    g.fillCircle(cx - 4, cy - 2, 1, kColorBg);
    g.fillCircle(cx + 1, cy - 2, 1, kColorBg);
    g.fillCircle(cx + 6, cy - 2, 1, kColorBg);
}

void glyphPerson(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.fillCircle(cx, cy - 5, 5, kColorAccent);
    g.fillRoundRect(cx - 9, cy + 2, 18, 9, 4, kColorAccent);
}

void glyphLock(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.drawRoundRect(cx - 6, cy - 11, 12, 12, 5, kColorAccent);
    g.drawRoundRect(cx - 5, cy - 10, 10, 10, 4, kColorAccent);
    g.fillRoundRect(cx - 9, cy - 3, 18, 13, 2, kColorAccent);
    g.fillRect(cx - 1, cy + 1, 3, 5, kColorBg);
}

void glyphStart(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.drawCircle(cx, cy, 10, kColorAccent);
    g.fillTriangle(cx - 4, cy - 6, cx - 4, cy + 6, cx + 7, cy, kColorAccent);
}

void glyphClock(Adafruit_GFX &g, int16_t cx, int16_t cy)
{
    g.drawCircle(cx, cy, 10, kColorAccent);
    g.drawCircle(cx, cy, 9, kColorAccent);
    g.drawLine(cx, cy, cx, cy - 6, kColorAccent);
    g.drawLine(cx, cy, cx + 5, cy + 2, kColorAccent);
}

using Glyph = void (*)(Adafruit_GFX &, int16_t, int16_t);
enum GlyphId : uint8_t { kGAi, kGWifi, kGSun, kGSpeaker, kGPen, kGChip, kGRole, kGPerson, kGLock, kGStart, kGClock };
const Glyph kGlyphs[] = {glyphAi, glyphWifi, glyphSun, glyphSpeaker, glyphPen, glyphChip,
                         glyphRole, glyphPerson, glyphLock, glyphStart, glyphClock};

String uptimeText(uint32_t sec)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%luh %02lum", (unsigned long)(sec / 3600), (unsigned long)(sec / 60 % 60));
    return String(buf);
}

}  // namespace

// ---------------------------------------------------------------- shared slider

namespace settings_layout {

void drawSlider(Adafruit_GFX &gfx, const char *label, uint8_t percent)
{
    gfx.fillRect(0, 26, kScreenW, kSliderY + kSliderH + 20 - 26, kColorBg);
    gfx.setTextSize(2);
    gfx.setTextColor(kColorText);
    gfx.setCursor(10, 34);
    gfx.print(label);
    const String value = String(percent) + "%";
    gfx.setTextSize(3);
    gfx.setTextColor(kColorAccent);
    gfx.setCursor(kScreenW - 10 - (int16_t)value.length() * kFontW * 3, 30);
    gfx.print(value);
    const int16_t lit = (int16_t)((percent + kSegments - 1) / kSegments);  // 0..10
    for (int16_t i = 0; i < kSegments; ++i) {
        const int16_t x = kSliderX + i * (kSegmentW + kSegmentGap);
        gfx.fillRoundRect(x, kSliderY, kSegmentW, kSliderH, 4, i < lit ? kColorAccent : kColorDivider);
    }
    gfx.setTextSize(1);
    gfx.setTextColor(kColorMuted);
    gfx.setCursor(kSliderX, kSliderY + kSliderH + 6);
    gfx.print("0");
    gfx.setCursor(kSliderX + kSliderW / 2 - 6, kSliderY + kSliderH + 6);
    gfx.print("50");
    gfx.setCursor(kSliderX + kSliderW - 18, kSliderY + kSliderH + 6);
    gfx.print("100");
}

uint8_t sliderValue(int16_t x, uint8_t step, uint8_t minimum)
{
    const int32_t raw = (int32_t)(x - kSliderX) * 100 / kSliderW;
    int32_t percent = (raw + step / 2) / step * step;
    percent = percent < minimum ? minimum : (percent > 100 ? 100 : percent);
    return (uint8_t)percent;
}

bool onSlider(int16_t y)
{
    return y >= kSliderY - kSliderHitPad && y <= kSliderY + kSliderH + kSliderHitPad;
}

}  // namespace settings_layout

SettingsUi::SettingsUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
}

void SettingsUi::show(const SettingsInfo &info, const DeviceSettings &device)
{
    info_ = info;
    device_ = device;
    page_ = Page::List;
    unlocked_ = false;
    armedRow_ = -1;
    visible_ = true;
    draw();
}

void SettingsUi::resume(const SettingsInfo &info, const DeviceSettings &device)
{
    info_ = info;
    device_ = device;
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

bool SettingsUi::sliderActive() const
{
    return page_ == Page::Display || page_ == Page::Sound;
}

void SettingsUi::draw()
{
    gfx_.fillScreen(kColorBg);
    switch (page_) {
    case Page::List: drawList(); break;
    case Page::Model: drawModel(); break;
    case Page::Display: drawDisplay(); break;
    case Page::About: drawAbout(); break;
    case Page::Persona: drawPersona(); break;
    case Page::Sound: drawSound(); break;
    case Page::Profile: drawProfile(); break;
    case Page::Security: drawSecurity(); break;
    case Page::Startup: drawStartup(); break;
    case Page::Clock: drawClock(); break;
    case Page::WifiMenu: drawWifiMenu(); break;
    }
}

// ---------------------------------------------------------------- list

void SettingsUi::drawRow(size_t row, size_t glyph, const char *label, const String &value, uint16_t valueColor)
{
    const int16_t top = kHeaderH + (int16_t)row * kRowH;
    const int16_t cy = top + kRowH / 2;
    gfx_.fillRect(0, top, kScreenW, kRowH, kColorBg);
    kGlyphs[glyph](gfx_, kRowIconX, cy);
    gfx_.setTextSize(2);
    gfx_.setTextColor(kColorText);
    gfx_.setCursor(kRowLabelX, cy - kFontH + 1);
    gfx_.print(label);
    const size_t labelChars = strlen(label);
    const size_t room = labelChars * 2 + 2 < 40 ? 40 - labelChars * 2 - 2 : 4;
    const String v = ellipsize(value, room < 18 ? room : 18);
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
    drawBackHeader(gfx_, kTitle, listPage_ == 0 ? "Trang 1/2  >" : "<  Trang 2/2", kColorAccent);
    if (listPage_ == 0) {
        drawRow(0, kGAi, "AI Model", info_.hasApiKey ? String("Da lien ket") : String("Chua co key"),
                info_.hasApiKey ? kColorText : kColorSystem);
        drawRow(1, kGRole, "Vai tro AI", device_.lockProfile ? String("(khoa)") : String("chon"), kColorText);
        drawRow(2, kGWifi, "Wi-Fi", info_.wifiConnected ? info_.ssid : String("Chua ket noi"),
                info_.wifiConnected ? kColorText : kColorSystem);
        drawRow(3, kGSun, "Man hinh", String(device_.brightness) + "%" + (device_.lightTheme ? " Sang" : " Toi"),
                kColorText);
        drawRow(4, kGSpeaker, "Am thanh",
                !info_.hasSpeaker ? String("Khong co loa")
                                  : (device_.sound ? String("Bat ") + String(device_.volume) + "%" : String("Tat")),
                info_.hasSpeaker ? kColorText : kColorMuted);
        drawRow(5, kGPen, "But cam ung", "Hieu chinh", kColorText);
        return;
    }
    drawRow(0, kGPerson, "Ca nhan", info_.userName.length() > 0 ? info_.userName : String("chua nhap ten"),
            kColorText);
    drawRow(1, kGLock, "Bao mat", info_.pinSet ? String("PIN da dat") : String("PIN chua dat"), kColorText);
    static const char *const kStartNames[] = {"Man hinh chinh", "Claude", "Chat", "Dong ho", "Lich am",
                                              "Radio",          "Nhac SD", "OBD-II", "Ap suat lop"};
    drawRow(2, kGStart, "Tu khoi dong", kStartNames[(size_t)device_.autoStart % (size_t)AutoStart::Count],
            kColorText);
    drawRow(3, kGClock, "Dong ho", device_.autoTimezone ? String("Tu dong ") + info_.timezoneText : info_.timezoneText,
            kColorText);
    drawRow(4, kGChip, "Thiet bi", info_.board, kColorText);
}

SettingsAction SettingsUi::listTap(size_t row)
{
    if (listPage_ == 0) {
        switch (row) {
        case 0: return open(Page::Model);
        case 1: return open(Page::Persona);
        case 2: return open(Page::WifiMenu);
        case 3: return open(Page::Display);
        case 4: return open(Page::Sound);
        case 5: return SettingsAction::Calibrate;
        default: return SettingsAction::None;
        }
    }
    switch (row) {
    case 0: return open(Page::Profile);
    case 1: return open(Page::Security);
    case 2: return open(Page::Startup);
    case 3: return open(Page::Clock);
    case 4: return open(Page::About);
    default: return SettingsAction::None;
    }
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
        gfx_.setTextColor(kColorText);
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
    drawSlider(gfx_, "Do sang", device_.brightness);
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
    const String rotate = String("Xoay: ") + (device_.flipped ? "180 do" : "0 do");
    const String off = "Tat sau: " + screenOffText();
    const String theme = String("Giao dien: ") + (device_.lightTheme ? "Sang" : "Toi");
    const String accent = "Mau " + String(device_.accent + 1) + "/" + String(kAccentCount);
    drawButton(gfx_, kRotateButton, rotate.c_str(), kColorButton, kColorWhite, 1);
    drawButton(gfx_, kScreenOffButton, off.c_str(), kColorButton, kColorWhite, 1);
    drawButton(gfx_, kThemeButton, theme.c_str(), kColorButton, kColorWhite, 1);
    drawButton(gfx_, kAccentButton, accent.c_str(), accentColor(device_.accent), kColorWhite, 1);
    drawCentered(gfx_, "Keo but tren thanh de chinh do sang", 0, kScreenW, 224, 1, kColorMuted);
}

SettingsAction SettingsUi::setBrightnessFrom(int16_t x)
{
    const uint8_t percent = sliderValue(x, kBrightnessStep, kBrightnessMin);
    if (percent == device_.brightness) {
        return SettingsAction::None;
    }
    device_.brightness = percent;
    drawBrightness();
    return SettingsAction::DeviceChanged;
}

SettingsAction SettingsUi::displayTap(int16_t x, int16_t y)
{
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
        draw();
        return SettingsAction::DeviceChanged;
    }
    if (kThemeButton.contains(x, y)) {
        device_.lightTheme = !device_.lightTheme;
        return SettingsAction::ThemeChanged;
    }
    if (kAccentButton.contains(x, y)) {
        device_.accent = (uint8_t)((device_.accent + 1) % kAccentCount);
        return SettingsAction::ThemeChanged;
    }
    return handleDrag(x, y);
}

// ---------------------------------------------------------------- about

void SettingsUi::drawAbout()
{
    drawBackHeader(gfx_, "Thiet bi");
    int16_t y = 26;
    auto line = [&](const char *label, const String &value) {
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorMuted);
        gfx_.setCursor(10, y);
        gfx_.print(label);
        gfx_.setTextColor(kColorText);
        gfx_.setCursor(110, y);
        gfx_.print(ellipsize(value, 34));
        y += 16;
    };
    line("Board", info_.board);
    line("Phan mem", FW_NAME " " FW_VERSION);
    line("Ma thiet bi", info_.deviceCode);
    line("MAC", info_.mac);
    line("Wi-Fi", info_.wifiConnected ? info_.ssid + "  " + info_.ip : String("chua ket noi"));
    line("Model", info_.model);
    line("App da luu", String((unsigned)info_.appCount));
    line("RAM trong", String(info_.freeHeap / 1024) + " KB (khoi lon nhat " + String(info_.maxHeapBlock / 1024) +
                          " KB)");
    line("PSRAM trong", info_.freePsram > 0 ? String(info_.freePsram / 1024) + " KB" : String("khong co"));
    line("Da chay", uptimeText(info_.uptimeSec));
    line("Loa", info_.hasSpeaker ? "co" : "khong");
    line("Khoi dong do", info_.resetReason);
    drawCentered(gfx_, "Lenh Serial: /status /help", 0, kScreenW, 222, 1, kColorMuted);
}

// ---------------------------------------------------------------- input

bool SettingsUi::pageLocked(Page page) const
{
    if (unlocked_ || !info_.pinSet) {
        return false;
    }
    if (page == Page::Security) {
        return true;  // changing the locks always needs the PIN
    }
    return device_.lockProfile && (page == Page::Persona || page == Page::Profile);
}

SettingsAction SettingsUi::open(Page page)
{
    if (pageLocked(page)) {
        pendingUnlock_ = page;
        return SettingsAction::RequestUnlock;
    }
    page_ = page;
    armedRow_ = -1;
    if (page == Page::Persona) {
        personaTop_ = device_.persona / kOptionRows * kOptionRows;
    }
    draw();
    return SettingsAction::None;
}

void SettingsUi::unlock()
{
    unlocked_ = true;
    visible_ = true;
    page_ = pendingUnlock_;
    draw();
}

SettingsAction SettingsUi::back()
{
    if (page_ == Page::List) {
        return SettingsAction::Back;
    }
    page_ = Page::List;
    armedRow_ = -1;
    draw();
    return SettingsAction::None;
}

SettingsAction SettingsUi::handleTouch(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        return back();
    }
    const size_t optionRow = y >= kOptionTop ? (size_t)((y - kOptionTop) / kOptionH) : kOptionRows;
    switch (page_) {
    case Page::List:
        if (kListPageHit.contains(x, y)) {
            listPage_ = (listPage_ + 1) % kListPages;
            draw();
            return SettingsAction::None;
        }
        if (y < kHeaderH) {
            return SettingsAction::None;
        }
        return listTap((size_t)((y - kHeaderH) / kRowH));
    case Page::Model:
        return y >= kModelRowTop && y < kModelRowBottom ? SettingsAction::NextModel : SettingsAction::None;
    case Page::Display: return displayTap(x, y);
    case Page::Persona: return personaTap(x, y);
    case Page::Sound: return soundTap(x, y);
    case Page::Profile: return profileTap(optionRow);
    case Page::Security: return securityTap(optionRow);
    case Page::Startup: return startupTap(x, y);
    case Page::Clock: return clockTap(optionRow);
    case Page::WifiMenu: return wifiMenuTap(optionRow);
    case Page::About: break;
    }
    return SettingsAction::None;
}

SettingsAction SettingsUi::handleDrag(int16_t x, int16_t y)
{
    if (!onSlider(y)) {
        return SettingsAction::None;
    }
    if (page_ == Page::Display) {
        return setBrightnessFrom(x);
    }
    if (page_ == Page::Sound) {
        return setVolumeFrom(x);
    }
    return SettingsAction::None;
}
