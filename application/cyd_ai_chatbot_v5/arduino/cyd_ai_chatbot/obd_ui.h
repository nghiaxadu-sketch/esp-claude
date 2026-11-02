// "OBD-II": car diagnostics with an ELM327 adapter. Pages: connect (Bluetooth LE or the adapter's Wi-Fi,
// last adapter, sample data), overview gauges (speed or RPM first), all sensors, fault codes (read stored /
// pending / permanent, clear after a second tap) and info (VIN, protocol, battery voltage).
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <vector>

#include "obd_link.h"
#include "ui_common.h"

enum class ObdAction : uint8_t { None, Exit, NeedWifiPassword };

class ObdUi {
public:
    explicit ObdUi(Adafruit_GFX &gfx);

    // autoConnect: go straight to the adapter used last time (Settings > Tu khoi dong = OBD).
    void show(bool autoConnect);
    bool visible() const { return visible_; }
    // Leaves the screen: closes the adapter connection and turns Bluetooth off.
    void hide();

    ObdAction press(int16_t x, int16_t y);
    ObdAction back();
    void loop(uint32_t nowMs);

    // The Wi-Fi adapter network that needs a password (after NeedWifiPassword), and the answer.
    const String &pendingSsid() const { return pendingSsid_; }
    void connectWifi(const String &ssid, const String &password);

private:
    enum class Page : uint8_t { Connect, Overview, Sensors, Dtc, Info };
    enum class Link : uint8_t { Ble, Wifi };
    enum class View : uint8_t { OverviewSpeed, OverviewRpm, AllSensors, Count };
    struct Candidate {
        String name;  // ASCII
        String address;  // BLE address or SSID
        uint8_t addressType;
        int rssi;
        bool open;  // Wi-Fi without password
        bool likely;  // name looks like an OBD adapter
    };
    static constexpr size_t kConnectRows = 4;
    static constexpr size_t kSensorRows = 8;

    void draw();
    void drawConnect();
    void drawCandidates();
    void drawOverview(bool full);
    void drawSensors();
    void drawDtc();
    void drawInfo();
    void drawTabs();
    void drawStatusLine();
    void startScan();
    void collectAdverts();
    void connectTo(const Candidate &c);
    void connectLast();
    void onConnected();
    void saveLast(const String &record);
    String valueText(uint8_t pid) const;

    Adafruit_GFX &gfx_;
    ObdSession session_;
    Page page_ = Page::Connect;
    Link link_ = Link::Ble;
    View view_ = View::OverviewSpeed;
    bool visible_ = false;
    std::vector<Candidate> candidates_;
    String lastRecord_;  // "B|address|type|name" or "W|ssid|password"
    String pendingSsid_;
    String connectNote_;
    size_t sensorTop_ = 0;
    bool clearArmed_ = false;
    bool wasRunning_ = false;
    ObdState shownState_ = ObdState::Idle;
    String shownStatus_;
    uint32_t lastDrawMs_ = 0;
    uint32_t scanStartMs_ = 0;
    bool dtcsShown_ = false;
};
