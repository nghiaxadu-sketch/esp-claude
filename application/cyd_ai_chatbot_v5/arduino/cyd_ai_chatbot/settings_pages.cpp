// Detail pages of Settings added for the Lily OS features: AI role, sound, profile, security (PIN),
// start-up screen, clock / time zone and the saved Wi-Fi networks.
#include "personas.h"
#include "settings_ui.h"
#include "settings_ui_internal.h"

using namespace settings_layout;
using namespace ui;

namespace {

constexpr uint8_t kVolumeStep = 10;
constexpr int16_t kPersonaFooterY = kScreenH - kFooterH;
const char *const kStartNames[] = {"Man hinh chinh", "Claude (mat AI)", "Chat", "Dong ho", "Lich am",
                                   "Radio",          "Nhac the SD",    "OBD-II", "Ap suat lop"};
constexpr size_t kStartCount = sizeof(kStartNames) / sizeof(kStartNames[0]);

}  // namespace

void SettingsUi::drawOption(size_t row, const char *label, const String &value, bool highlight)
{
    const int16_t y = kOptionTop + (int16_t)row * kOptionH;
    gfx_.fillRoundRect(kMargin, y, kScreenW - 2 * kMargin, kOptionH - 3, 4, highlight ? kColorAccent : kColorRow);
    gfx_.setTextSize(1);
    gfx_.setTextColor(highlight ? kColorWhite : kColorText);
    gfx_.setCursor(10, y + 10);
    gfx_.print(label);
    const String v = ellipsize(value, 26);
    gfx_.setTextColor(highlight ? kColorWhite : kColorAccent);
    gfx_.setCursor(kScreenW - 10 - (int16_t)v.length() * kFontW, y + 10);
    gfx_.print(v);
}

// ---------------------------------------------------------------- AI role

void SettingsUi::drawPersona()
{
    drawBackHeader(gfx_, "Vai tro AI", String(device_.persona + 1) + "/" + String((unsigned)personas::count()));
    for (size_t row = 0; row < kOptionRows && personaTop_ + row < personas::count(); ++row) {
        const size_t i = personaTop_ + row;
        drawOption(row, personas::get(i).name, i == device_.persona ? "dang dung" : "", i == device_.persona);
    }
    drawButton(gfx_, footerSlot(0, 3), "Len", kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(1, 3), "Xuong", kColorButton, kColorWhite);
    const String lang = String("Tra loi: ") + personas::languageName((personas::Language)device_.language);
    drawButton(gfx_, footerSlot(2, 3), lang.c_str(), kColorAccent, kColorWhite, 1);
}

SettingsAction SettingsUi::personaTap(int16_t x, int16_t y)
{
    if (y >= kPersonaFooterY) {
        if (footerSlot(0, 3).contains(x, y) && personaTop_ > 0) {
            personaTop_ = personaTop_ >= kOptionRows ? personaTop_ - kOptionRows : 0;
        } else if (footerSlot(1, 3).contains(x, y) && personaTop_ + kOptionRows < personas::count()) {
            personaTop_ += kOptionRows;
        } else if (footerSlot(2, 3).contains(x, y)) {
            device_.language = (uint8_t)((device_.language + 1) % personas::kLanguageCount);
            draw();
            return SettingsAction::DeviceChanged;
        }
        draw();
        return SettingsAction::None;
    }
    if (y < kOptionTop) {
        return SettingsAction::None;
    }
    const size_t i = personaTop_ + (size_t)((y - kOptionTop) / kOptionH);
    if (i < personas::count() && (y - kOptionTop) / kOptionH < (int)kOptionRows) {
        device_.persona = (uint8_t)i;
        draw();
        return SettingsAction::DeviceChanged;
    }
    return SettingsAction::None;
}

// ---------------------------------------------------------------- sound

void SettingsUi::drawVolume()
{
    drawSlider(gfx_, "Am luong", device_.volume);
}

void SettingsUi::drawSound()
{
    drawBackHeader(gfx_, "Am thanh", info_.hasSpeaker ? "" : "Khong co loa", kColorSystem);
    drawVolume();
    const Rect soundBtn = {8, 128, 150, 36};
    const Rect chimeBtn = {162, 128, 150, 36};
    const Rect testBtn = {8, 172, 304, 36};
    drawButton(gfx_, soundBtn, device_.sound ? "Am thanh: Bat" : "Am thanh: Tat", device_.sound ? kColorOk : kColorButton,
               kColorWhite, 1);
    drawButton(gfx_, chimeBtn, device_.hourlyChime ? "Bao gio: Bat" : "Bao gio: Tat",
               device_.hourlyChime ? kColorOk : kColorButton, kColorWhite, 1);
    drawButton(gfx_, testBtn, "Thu loa", kColorButton, kColorWhite);
    drawCentered(gfx_, "Am luong dung cho tieng bip, Radio va Nhac SD", 0, kScreenW, 224, 1, kColorMuted);
}

SettingsAction SettingsUi::setVolumeFrom(int16_t x)
{
    const uint8_t percent = sliderValue(x, kVolumeStep, 0);
    if (percent == device_.volume) {
        return SettingsAction::None;
    }
    device_.volume = percent;
    drawVolume();
    return SettingsAction::DeviceChanged;
}

SettingsAction SettingsUi::soundTap(int16_t x, int16_t y)
{
    if (y >= 128 && y < 164) {
        if (x < kScreenW / 2) {
            device_.sound = !device_.sound;
        } else {
            device_.hourlyChime = !device_.hourlyChime;
        }
        draw();
        return SettingsAction::DeviceChanged;
    }
    if (y >= 172 && y < 208) {
        return SettingsAction::TestSound;
    }
    return handleDrag(x, y);
}

// ---------------------------------------------------------------- profile

void SettingsUi::drawProfile()
{
    drawBackHeader(gfx_, "Ca nhan");
    drawOption(0, "Ten ban", info_.userName.length() > 0 ? info_.userName : String("Chua nhap"));
    drawOption(1, "Vi tri (thoi tiet, ban do)",
               info_.locationName.length() > 0 ? info_.locationName : String("Hai Phong (mac dinh)"));
    drawOption(2, "Tu dong chao khi bat may", device_.autoGreet ? "Bat" : "Tat");
    drawOption(3, "Ngon ngu tra loi", personas::languageName((personas::Language)device_.language));
    drawOption(4, "Vai tro AI", personas::get(device_.persona).name);
    drawCentered(gfx_, "Ten va vai tro duoc gui kem cho Claude", 0, kScreenW, 224, 1, kColorMuted);
}

SettingsAction SettingsUi::profileTap(size_t row)
{
    switch (row) {
    case 0: return SettingsAction::EditName;
    case 1: return SettingsAction::EditLocation;
    case 2:
        device_.autoGreet = !device_.autoGreet;
        draw();
        return SettingsAction::DeviceChanged;
    case 3:
        device_.language = (uint8_t)((device_.language + 1) % personas::kLanguageCount);
        draw();
        return SettingsAction::DeviceChanged;
    case 4: return open(Page::Persona);
    default: return SettingsAction::None;
    }
}

// ---------------------------------------------------------------- security

void SettingsUi::drawSecurity()
{
    drawBackHeader(gfx_, "Bao mat");
    drawOption(0, info_.pinSet ? "Doi ma PIN" : "Dat ma PIN (6 so)", info_.pinSet ? "Da dat" : "Chua dat");
    drawOption(1, "Khoa app (can PIN de mo)", device_.lockApps ? "Bat" : "Tat");
    drawOption(2, "Khoa profile (vai tro, ten)", device_.lockProfile ? "Bat" : "Tat");
    drawCentered(gfx_, info_.pinSet ? "Khoa app: Claude, Chat, Tro choi, app da luu..." : "Dat ma PIN truoc khi bat khoa",
                 0, kScreenW, 224, 1, kColorMuted);
}

SettingsAction SettingsUi::securityTap(size_t row)
{
    switch (row) {
    case 0: return info_.pinSet ? SettingsAction::ChangePin : SettingsAction::SetPin;
    case 1:
    case 2:
        if (!info_.pinSet) {
            return SettingsAction::SetPin;
        }
        if (row == 1) {
            device_.lockApps = !device_.lockApps;
        } else {
            device_.lockProfile = !device_.lockProfile;
        }
        draw();
        return SettingsAction::DeviceChanged;
    default: return SettingsAction::None;
    }
}

// ---------------------------------------------------------------- start-up screen

void SettingsUi::drawStartup()
{
    drawBackHeader(gfx_, "Tu khoi dong", "Mo sau khi bat may");
    for (size_t i = 0; i < kOptionRows && i < kStartCount; ++i) {
        drawOption(i, kStartNames[i], (size_t)device_.autoStart == i ? "dang chon" : "", (size_t)device_.autoStart == i);
    }
    // The last choices do not fit in 7 rows: show them in the footer.
    for (size_t i = kOptionRows; i < kStartCount; ++i) {
        const bool chosen = (size_t)device_.autoStart == i;
        drawButton(gfx_, footerSlot(i - kOptionRows, kStartCount - kOptionRows), kStartNames[i],
                   chosen ? kColorAccent : kColorButton, kColorWhite, 1);
    }
}

SettingsAction SettingsUi::startupTap(int16_t x, int16_t y)
{
    size_t choice = kStartCount;
    if (y >= kPersonaFooterY) {
        for (size_t i = kOptionRows; i < kStartCount; ++i) {
            if (footerSlot(i - kOptionRows, kStartCount - kOptionRows).contains(x, y)) {
                choice = i;
            }
        }
    } else if (y >= kOptionTop) {
        choice = (size_t)((y - kOptionTop) / kOptionH);
    }
    if (choice >= kStartCount) {
        return SettingsAction::None;
    }
    device_.autoStart = (AutoStart)choice;
    draw();
    return SettingsAction::DeviceChanged;
}

// ---------------------------------------------------------------- clock

void SettingsUi::drawClock()
{
    drawBackHeader(gfx_, "Dong ho");
    drawOption(0, "Mui gio", device_.autoTimezone ? "Tu dong theo IP" : "UTC+7 (Viet Nam)");
    drawOption(1, "Dang dung", info_.timezoneText);
    drawOption(2, "Bao gio (6h - 22h)", device_.hourlyChime ? "Bat" : "Tat");
    drawOption(3, "Mat dong ho", device_.clockFace == 1 ? "Kim" : "So");
    drawOption(4, "Dong bo gio ngay", "cham");
    drawCentered(gfx_, "Gio lay tu Internet (NTP), can Wi-Fi", 0, kScreenW, 224, 1, kColorMuted);
}

SettingsAction SettingsUi::clockTap(size_t row)
{
    switch (row) {
    case 0:
        device_.autoTimezone = !device_.autoTimezone;
        draw();
        return SettingsAction::SyncTime;
    case 2:
        device_.hourlyChime = !device_.hourlyChime;
        draw();
        return SettingsAction::DeviceChanged;
    case 3:
        device_.clockFace = device_.clockFace == 1 ? 0 : 1;
        draw();
        return SettingsAction::DeviceChanged;
    case 4: return SettingsAction::SyncTime;
    default: return SettingsAction::None;
    }
}

// ---------------------------------------------------------------- Wi-Fi

void SettingsUi::drawWifiMenu()
{
    drawBackHeader(gfx_, "Wi-Fi", info_.wifiConnected ? info_.ssid : String("chua ket noi"),
                   info_.wifiConnected ? kColorOk : kColorSystem);
    drawOption(0, "Quet va chon mang", ">");
    drawOption(1, "Cau hinh qua dien thoai (diem phat)", ">");
    drawOption(2, "Xoa Wi-Fi & cau hinh lai", armedRow_ == 2 ? "Cham lan nua de xoa" : "", armedRow_ == 2);
    for (size_t i = 0; i < info_.savedWifi.size() && i + 3 < kOptionRows; ++i) {
        const bool armed = armedRow_ == (int)(i + 3);
        drawOption(i + 3, info_.savedWifi[i].c_str(), armed ? "Cham lan nua de quen" : "da luu - cham de quen", armed);
    }
}

SettingsAction SettingsUi::wifiMenuTap(size_t row)
{
    if (row == 0) {
        return SettingsAction::Wifi;
    }
    if (row == 1) {
        return SettingsAction::StartApSetup;
    }
    if (row == 2 || (row >= 3 && row - 3 < info_.savedWifi.size() && row < kOptionRows)) {
        if (armedRow_ != (int)row) {
            armedRow_ = (int)row;
            draw();
            return SettingsAction::None;
        }
        armedRow_ = -1;
        if (row == 2) {
            return SettingsAction::ResetWifi;
        }
        forgetIndex_ = row - 3;
        return SettingsAction::ForgetWifi;
    }
    return SettingsAction::None;
}
