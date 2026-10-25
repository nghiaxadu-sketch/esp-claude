// Minimal XPT2046 resistive touch driver on a shared SPI bus (no extra library needed).
#pragma once

#include <Arduino.h>
#include <SPI.h>

// Maps raw ADC readings to screen pixels. Produced by the 3-point calibration in the sketch.
struct TouchCalibration {
    static constexpr uint32_t kMagic = 0x43594431;  // "CYD1"

    uint32_t magic = 0;
    bool swapXy = false;  // raw Y axis follows screen X
    // Raw values (after the optional swap) measured at two known screen points per axis.
    int16_t rawX0 = 0, rawX1 = 0;
    int16_t screenX0 = 0, screenX1 = 0;
    int16_t rawY0 = 0, rawY1 = 0;
    int16_t screenY0 = 0, screenY1 = 0;

    bool valid() const
    {
        return magic == kMagic && rawX0 != rawX1 && rawY0 != rawY1;
    }
};

struct TouchRaw {
    int16_t x;
    int16_t y;
    int16_t pressure;
};

class TouchXpt2046 {
public:
    TouchXpt2046(SPIClass &spi, int8_t csPin, uint16_t screenWidth, uint16_t screenHeight);

    void begin();
    void setCalibration(const TouchCalibration &cal);
    bool calibrated() const { return cal_.valid(); }

    // Median-filtered raw reading; returns false when the panel is not pressed.
    bool readRaw(TouchRaw &raw);
    // Calibrated screen position; returns false when not pressed or not calibrated.
    bool read(int16_t &x, int16_t &y);

private:
    uint16_t transfer12(uint8_t command);

    SPIClass &spi_;
    int8_t csPin_;
    uint16_t width_;
    uint16_t height_;
    TouchCalibration cal_;
};
