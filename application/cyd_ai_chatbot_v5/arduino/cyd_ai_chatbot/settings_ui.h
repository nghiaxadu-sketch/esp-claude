// Settings: two pages of rows (AI model, AI role, Wi-Fi, display, sound, stylus / profile, security,
// start-up screen, clock, device) and their detail pages. The AI model page shows the web address as a QR
// code; the display page has a brightness slider that follows the stylus, rotation, screen-off timer, dark /
// light theme and the accent color ("Giao dien 1..20").
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <vector>

#include "qr_code.h"
#include "settings_store.h"
#include "ui_common.h"

// Everything the pages show that belongs to other modules; filled by the main flow (ASCII strings).
struct SettingsInfo {
    String model;
    bool hasApiKey = false;
    bool wifiConnected = false;
    String ssid;
    String ip;
    String board;
    bool hasSpeaker = false;
    uint32_t freeHeap = 0;
    uint32_t maxHeapBlock = 0;  // largest free block: a TLS connection needs ~24 KB in one piece
    uint32_t freePsram = 0;
    size_t appCount = 0;
    uint32_t uptimeSec = 0;
    String resetReason;  // why the board last restarted (power on, crash, watchdog...)
    String mac;
    String deviceCode;   // short id shown in Thiet bi
    std::vector<String> savedWifi;  // ASCII names, most recent first
    String userName;
    String locationName;
    bool pinSet = false;
    String timezoneText;  // "UTC+7"
};

enum class SettingsAction : uint8_t {
    None,
    Back,            // leave Settings
    Wifi,            // open the Wi-Fi scan screen
    Calibrate,       // run the stylus calibration
    DeviceChanged,   // device() changed: apply and save it
    ThemeChanged,    // device() theme / accent changed: apply, save and redraw everything
    NextModel,       // switch to the next Claude model
    EditName,        // keyboard for the user's name
    EditLocation,    // keyboard for the weather / map location
    SetPin,          // PIN pad: new PIN
    ChangePin,       // PIN pad: old then new PIN
    RequestUnlock,   // a locked page was tapped: verify the PIN, then call unlock()
    StartApSetup,    // Wi-Fi setup from a phone through the board's own hotspot
    ResetWifi,       // forget every saved network
    ForgetWifi,      // forget forgetSsidIndex()
    SyncTime,        // read the time zone now and resync NTP
    TestSound,       // play a test beep at the chosen volume
};

class SettingsUi {
public:
    explicit SettingsUi(Adafruit_GFX &gfx);

    void show(const SettingsInfo &info, const DeviceSettings &device);
    // Comes back to the page that was open (after the keyboard or the PIN pad).
    void resume(const SettingsInfo &info, const DeviceSettings &device);
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }
    // Redraws the current page with fresh info (after a model change, Wi-Fi reconnect...).
    void update(const SettingsInfo &info);
    // Redraws everything (after a theme change).
    void redraw() { draw(); }

    SettingsAction handleTouch(int16_t x, int16_t y);
    // Stylus moving while pressed (brightness / volume sliders).
    SettingsAction handleDrag(int16_t x, int16_t y);
    // "<" or a back swipe.
    SettingsAction back();
    const DeviceSettings &device() const { return device_; }
    size_t forgetSsidIndex() const { return forgetIndex_; }
    // After RequestUnlock succeeded: opens the page that asked for it.
    void unlock();
    bool sliderActive() const;

private:
    enum class Page : uint8_t { List, Model, Display, About, Persona, Sound, Profile, Security, Startup, Clock, WifiMenu };
    static constexpr size_t kRows = 6;
    static constexpr size_t kListPages = 2;

    void draw();
    void drawList();
    void drawRow(size_t row, size_t glyph, const char *label, const String &value, uint16_t valueColor);
    void drawModel();
    void drawQr(const String &text, int16_t x, int16_t y, int16_t maxSide);
    void drawDisplay();
    void drawBrightness();
    void drawAbout();
    void drawInfoLine(int16_t y, const char *label, const String &value, uint16_t color = ui::kColorText);
    SettingsAction setBrightnessFrom(int16_t x);
    String screenOffText() const;
    SettingsAction listTap(size_t row);
    SettingsAction displayTap(int16_t x, int16_t y);

    // settings_pages.cpp
    void drawPersona();
    void drawSound();
    void drawVolume();
    void drawProfile();
    void drawSecurity();
    void drawStartup();
    void drawClock();
    void drawWifiMenu();
    void drawOption(size_t row, const char *label, const String &value, bool highlight = false);
    SettingsAction personaTap(int16_t x, int16_t y);
    SettingsAction soundTap(int16_t x, int16_t y);
    SettingsAction setVolumeFrom(int16_t x);
    SettingsAction profileTap(size_t row);
    SettingsAction securityTap(size_t row);
    SettingsAction startupTap(int16_t x, int16_t y);
    SettingsAction clockTap(size_t row);
    SettingsAction wifiMenuTap(size_t row);
    bool pageLocked(Page page) const;
    SettingsAction open(Page page);

    Adafruit_GFX &gfx_;
    SettingsInfo info_;
    DeviceSettings device_ = {};
    Page page_ = Page::List;
    Page pendingUnlock_ = Page::List;
    size_t listPage_ = 0;
    size_t personaTop_ = 0;
    size_t forgetIndex_ = 0;
    int armedRow_ = -1;  // row waiting for a confirming second tap ("Xoa Wi-Fi")
    bool unlocked_ = false;
    bool visible_ = false;
    qr_code::Code qr_;
    String qrText_;
};
