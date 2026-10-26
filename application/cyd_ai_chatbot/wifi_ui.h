// Wi-Fi screen: list of scanned networks with signal bars; tap one to connect.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <vector>

#include "ui_common.h"
#include "wifi_manager.h"

enum class WifiAction : uint8_t { None, Rescan, Select, Back };

class WifiUi {
public:
    explicit WifiUi(Adafruit_GFX &gfx);

    // `status` is ASCII (e.g. "Da ket noi: NhaToi  192.168.1.5").
    void show(const std::vector<WifiNetwork> &networks, const String &status);
    void showScanning();
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }
    void setStatus(const String &status);

    WifiAction handleTouch(int16_t x, int16_t y);
    // Network chosen by the last Select.
    const WifiNetwork &selected() const { return networks_[selected_]; }

private:
    static constexpr size_t kFooterButtons = 4;

    void drawList();
    void drawStatus();
    void drawBars(int16_t x, int16_t y, uint8_t bars);
    int visibleRows() const;

    Adafruit_GFX &gfx_;
    std::vector<WifiNetwork> networks_;
    String status_;
    bool visible_ = false;
    size_t scrollTop_ = 0;
    size_t selected_ = 0;
    ui::Rect footer_[kFooterButtons];
};
