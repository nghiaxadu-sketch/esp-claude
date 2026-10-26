// Đèn LED RGB + còi (loa của CYD) theo mức cảnh báo. Không chặn (non-blocking): gọi tick() liên tục.
#pragma once
#include <Arduino.h>

#include "safety_logic.h"

class AlarmOutput {
public:
    void begin();
    // level: mức an toàn sạc; airBad: CO2 cao (LED xanh dương nhấp nháy khi sạc đang an toàn)
    void set(safety::Level level, bool silenced, bool airBad);
    void tick();
    void beepShort();  // tiếng bíp phản hồi khi chạm màn hình

private:
    void led(bool r, bool g, bool b);
    void tone(uint16_t freq);
    void quiet();

    safety::Level level_ = safety::LEVEL_SAFE;
    bool silenced_ = false;
    bool airBad_ = false;
    uint32_t phaseStartMs_ = 0;
    uint32_t beepUntilMs_ = 0;
    uint16_t currentFreq_ = 0;
};
