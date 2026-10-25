#include "touch_xpt2046.h"

#include <algorithm>

namespace {

constexpr uint32_t kTouchSpiHz = 2000000;  // XPT2046 max is ~2.5 MHz
constexpr uint8_t kCmdReadX = 0xD1;        // 12-bit, differential, keep powered
constexpr uint8_t kCmdReadY = 0x91;
constexpr uint8_t kCmdReadZ1 = 0xB1;
constexpr uint8_t kCmdReadZ2 = 0xC1;
constexpr uint8_t kCmdPowerDown = 0x90;    // last read with PD bits = 0 re-enables PENIRQ
constexpr int16_t kRawMax = 4095;
constexpr int16_t kPressureThreshold = 350;
constexpr int kSamples = 5;

int16_t median(int16_t (&v)[kSamples])
{
    std::sort(v, v + kSamples);
    return v[kSamples / 2];
}

int16_t mapAxis(int16_t raw, int16_t raw0, int16_t raw1, int16_t s0, int16_t s1, int16_t limit)
{
    const int32_t value = s0 + (int32_t)(raw - raw0) * (s1 - s0) / (raw1 - raw0);
    return (int16_t)std::min<int32_t>(std::max<int32_t>(value, 0), limit - 1);
}

}  // namespace

TouchXpt2046::TouchXpt2046(SPIClass &spi, int8_t csPin, uint16_t screenWidth, uint16_t screenHeight)
    : spi_(spi), csPin_(csPin), width_(screenWidth), height_(screenHeight)
{
}

void TouchXpt2046::begin()
{
    pinMode(csPin_, OUTPUT);
    digitalWrite(csPin_, HIGH);
}

void TouchXpt2046::setCalibration(const TouchCalibration &cal)
{
    cal_ = cal;
}

uint16_t TouchXpt2046::transfer12(uint8_t command)
{
    spi_.transfer(command);
    return spi_.transfer16(0) >> 3;
}

bool TouchXpt2046::readRaw(TouchRaw &raw)
{
    int16_t xs[kSamples];
    int16_t ys[kSamples];

    spi_.beginTransaction(SPISettings(kTouchSpiHz, MSBFIRST, SPI_MODE0));
    digitalWrite(csPin_, LOW);
    const int16_t z1 = transfer12(kCmdReadZ1);
    const int16_t z2 = transfer12(kCmdReadZ2);
    const int16_t pressure = z1 + kRawMax - z2;
    if (pressure >= kPressureThreshold) {
        transfer12(kCmdReadX);  // first conversion after switching channels is noisy
        for (int i = 0; i < kSamples; ++i) {
            xs[i] = transfer12(kCmdReadX);
            ys[i] = transfer12(kCmdReadY);
        }
    }
    transfer12(kCmdPowerDown);
    digitalWrite(csPin_, HIGH);
    spi_.endTransaction();

    if (pressure < kPressureThreshold) {
        return false;
    }
    raw.x = median(xs);
    raw.y = median(ys);
    raw.pressure = pressure;
    // Readings at the rails mean the finger lifted mid-sample.
    return raw.x > 0 && raw.x < kRawMax && raw.y > 0 && raw.y < kRawMax;
}

bool TouchXpt2046::read(int16_t &x, int16_t &y)
{
    TouchRaw raw;
    if (!cal_.valid() || !readRaw(raw)) {
        return false;
    }
    const int16_t a = cal_.swapXy ? raw.y : raw.x;
    const int16_t b = cal_.swapXy ? raw.x : raw.y;
    x = mapAxis(a, cal_.rawX0, cal_.rawX1, cal_.screenX0, cal_.screenX1, width_);
    y = mapAxis(b, cal_.rawY0, cal_.rawY1, cal_.screenY0, cal_.screenY1, height_);
    return true;
}
