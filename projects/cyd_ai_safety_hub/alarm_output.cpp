#include "alarm_output.h"

#include "config.h"

namespace {
constexpr uint8_t kSpeakerResolutionBits = 10;
constexpr uint16_t kToneLow = 1400;
constexpr uint16_t kToneHigh = 2200;
constexpr uint16_t kToneBeep = 2700;
}  // namespace

void AlarmOutput::begin() {
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    led(false, false, false);
    ledcAttach(PIN_SPEAKER, kToneHigh, kSpeakerResolutionBits);
    quiet();
}

void AlarmOutput::led(bool r, bool g, bool b) {
    // LED trên CYD nối kiểu anode chung -> LOW = sáng
    digitalWrite(PIN_LED_R, r ? LOW : HIGH);
    digitalWrite(PIN_LED_G, g ? LOW : HIGH);
    digitalWrite(PIN_LED_B, b ? LOW : HIGH);
}

void AlarmOutput::tone(uint16_t freq) {
    if (freq == currentFreq_) return;
    currentFreq_ = freq;
    ledcWriteTone(PIN_SPEAKER, freq);  // tự đặt duty 50%
}

void AlarmOutput::quiet() {
    currentFreq_ = 0;
    ledcWriteTone(PIN_SPEAKER, 0);  // freq 0 = tắt
}

void AlarmOutput::set(safety::Level level, bool silenced, bool airBad) {
    if (level != level_) phaseStartMs_ = millis();
    level_ = level;
    silenced_ = silenced;
    airBad_ = airBad;
}

void AlarmOutput::beepShort() { beepUntilMs_ = millis() + 40; }

void AlarmOutput::tick() {
    const uint32_t now = millis();
    const uint32_t t = now - phaseStartMs_;
    bool sound = false;
    uint16_t freq = kToneHigh;

    switch (level_) {
        case safety::LEVEL_SAFE: {
            // Xanh lá sáng nhẹ; CO2 cao -> nháy xanh dương
            const bool blueBlink = airBad_ && ((now / 1000) % 2 == 0);
            led(false, !blueBlink, blueBlink);
            break;
        }
        case safety::LEVEL_WATCH:
            led(true, true, false);  // vàng
            sound = t < 150;         // 1 tiếng bíp khi chuyển sang mức này
            break;
        case safety::LEVEL_DANGER:
            led(true, false, false);      // đỏ
            sound = (t % 2000) < 200;     // bíp mỗi 2 giây
            break;
        case safety::LEVEL_EMERGENCY: {
            const bool on = (now / 250) % 2 == 0;
            led(on, false, false);               // đỏ nhấp nháy
            sound = true;                        // còi hú 2 tông liên tục
            freq = ((now / 400) % 2) ? kToneLow : kToneHigh;
            break;
        }
    }

    if (silenced_) sound = false;
    if (!sound && now < beepUntilMs_) {
        sound = true;
        freq = kToneBeep;
    }
    if (sound) {
        tone(freq);
    } else if (currentFreq_ != 0) {
        quiet();
    }
}
