// "Ap suat lop" (TPMS): a car seen from above with the four tyres and the spare, each showing pressure,
// temperature, status and how long ago its sensor last reported. Pair a sensor to a tyre, swap two tyres,
// alert limits / sound / units, and sample data. Bluetooth listens only while this screen is open.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include "tpms_codec.h"
#include "ui_common.h"

enum class TpmsAction : uint8_t { None, Exit };

class TpmsUi {
public:
    explicit TpmsUi(Adafruit_GFX &gfx);

    void show();
    bool visible() const { return visible_; }
    void hide();

    TpmsAction press(int16_t x, int16_t y);
    TpmsAction back();
    void loop(uint32_t nowMs);

private:
    enum class Page : uint8_t { Tyres, Settings };
    enum class Mode : uint8_t { View, Pair, Swap };

    // Saved in NVS: pairing, limits and units.
    struct Stored {
        uint32_t magic;
        char ids[tpms::kSlotCount][tpms::kIdLength];
        tpms::Limits limits;
        uint8_t pressureUnit;
        uint8_t tempUnit;
        bool sound;
    };

    void load();
    void save();
    void draw();
    void drawTyres();
    void drawTyre(size_t slot);
    void drawStatusLine(const String &text, uint16_t color);
    void drawSettings();
    void drawFooter();
    ui::Rect tyreRect(size_t slot) const;
    void startListening();
    void processAdverts(uint32_t nowMs);
    void feedDemo(uint32_t nowMs);
    void alarm(uint32_t nowMs);

    Adafruit_GFX &gfx_;
    tpms::Tyres tyres_;
    tpms::Limits limits_;
    tpms::PressureUnit pressureUnit_ = tpms::PressureUnit::Bar;
    tpms::TempUnit tempUnit_ = tpms::TempUnit::Celsius;
    bool sound_ = true;
    Page page_ = Page::Tyres;
    Mode mode_ = Mode::View;
    int swapFirst_ = -1;
    bool demo_ = false;
    bool visible_ = false;
    String status_;
    String unknownSensor_;
    uint32_t lastRedrawMs_ = 0;
    uint32_t lastDemoMs_ = 0;
    uint32_t lastAlarmMs_ = 0;
    uint32_t lastPacketMs_ = 0;
};
