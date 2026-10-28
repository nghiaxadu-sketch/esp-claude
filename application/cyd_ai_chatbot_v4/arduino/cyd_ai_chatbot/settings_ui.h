// Settings: a list (AI model, Wi-Fi, display, sound, stylus calibration, device) and its detail pages.
// The AI model page shows the web address as a QR code; the display page has a brightness slider that
// follows the stylus, screen rotation and the screen-off timer.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

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
    uint32_t freePsram = 0;
    size_t appCount = 0;
    uint32_t uptimeSec = 0;
};

enum class SettingsAction : uint8_t {
    None,
    Back,           // leave Settings
    Wifi,           // open the Wi-Fi screen
    Calibrate,      // run the stylus calibration
    DeviceChanged,  // device() changed: apply and save it
    NextModel,      // switch to the next Claude model
};

class SettingsUi {
public:
    explicit SettingsUi(Adafruit_GFX &gfx);

    void show(const SettingsInfo &info, const DeviceSettings &device);
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }
    // Redraws the current page with fresh info (after a model change, Wi-Fi reconnect...).
    void update(const SettingsInfo &info);

    SettingsAction handleTouch(int16_t x, int16_t y);
    // Stylus moving while pressed (brightness slider).
    SettingsAction handleDrag(int16_t x, int16_t y);
    const DeviceSettings &device() const { return device_; }

private:
    enum class Page : uint8_t { List, Model, Display, About };
    static constexpr size_t kRows = 6;

    void draw();
    void drawList();
    void drawRow(size_t row, const char *label, const String &value, uint16_t valueColor);
    void drawModel();
    void drawQr(const String &text, int16_t x, int16_t y, int16_t maxSide);
    void drawDisplay();
    void drawBrightness();
    void drawAbout();
    void drawInfoLine(int16_t y, const char *label, const String &value, uint16_t color = ui::kColorText);
    SettingsAction setBrightnessFrom(int16_t x);
    String screenOffText() const;

    Adafruit_GFX &gfx_;
    SettingsInfo info_;
    DeviceSettings device_ = {};
    Page page_ = Page::List;
    bool visible_ = false;
    qr_code::Code qr_;
    String qrText_;
};
