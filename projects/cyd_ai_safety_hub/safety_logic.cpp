#include "safety_logic.h"

#include <stdio.h>
#include <string.h>

namespace safety {

namespace {

bool has(float v) { return !std::isnan(v); }

Level maxLevel(Level a, Level b) { return a > b ? a : b; }

}  // namespace

Assessment assess(const Inputs &in, const Thresholds &th) {
    Assessment a;
    int dangerSignals = 0;  // số tín hiệu đạt mức Nguy cơ trở lên

    if (has(in.chargerTempC)) {
        Level t = LEVEL_SAFE;
        if (in.chargerTempC >= th.tempEmergencyC) {
            t = LEVEL_EMERGENCY;
        } else if (in.chargerTempC >= th.tempDangerC) {
            t = LEVEL_DANGER;
        } else if (in.chargerTempC >= th.tempWarnC) {
            t = LEVEL_WATCH;
        }
        if (t != LEVEL_SAFE) a.reasons |= R_TEMP_HIGH;
        if (t >= LEVEL_DANGER) ++dangerSignals;
        a.level = maxLevel(a.level, t);
    }

    if (has(in.tempRiseCPerMin) && th.riseWarnCPerMin > 0) {
        Level r = LEVEL_SAFE;
        if (in.tempRiseCPerMin >= 2.0f * th.riseWarnCPerMin) {
            r = LEVEL_DANGER;
        } else if (in.tempRiseCPerMin >= th.riseWarnCPerMin) {
            r = LEVEL_WATCH;
        }
        if (r != LEVEL_SAFE) a.reasons |= R_TEMP_RISE;
        if (r >= LEVEL_DANGER) ++dangerSignals;
        a.level = maxLevel(a.level, r);
    }

    bool smokeDanger = false;
    if (has(in.smokePct)) {
        Level s = LEVEL_SAFE;
        if (in.smokePct >= th.smokeDangerPct) {
            s = LEVEL_DANGER;
            smokeDanger = true;
        } else if (in.smokePct >= th.smokeWarnPct) {
            s = LEVEL_WATCH;
        }
        if (s != LEVEL_SAFE) a.reasons |= R_SMOKE;
        if (s >= LEVEL_DANGER) ++dangerSignals;
        a.level = maxLevel(a.level, s);
    }

    if (has(in.currentA) && th.currentMaxA > 0) {
        Level c = LEVEL_SAFE;
        if (in.currentA > th.currentMaxA) {
            c = LEVEL_DANGER;
        } else if (in.currentA > 0.9f * th.currentMaxA) {
            c = LEVEL_WATCH;
        }
        if (c != LEVEL_SAFE) a.reasons |= R_OVERCURRENT;
        if (c >= LEVEL_DANGER) ++dangerSignals;
        a.level = maxLevel(a.level, c);
    }

    if (has(in.voltageV) && th.voltageMaxV > 0 && in.voltageV > th.voltageMaxV) {
        a.reasons |= R_OVERVOLTAGE;
        ++dangerSignals;
        a.level = maxLevel(a.level, LEVEL_DANGER);
    }

    if (in.sensorFault) {
        a.reasons |= R_SENSOR_FAULT;
        a.level = maxLevel(a.level, LEVEL_WATCH);
    }

    // Khẩn cấp: nhiều dấu hiệu nguy hiểm đồng thời, hoặc có khói kèm nhiệt độ cao
    const bool tempElevated = has(in.chargerTempC) && in.chargerTempC >= th.tempWarnC;
    if (dangerSignals >= 2 || (smokeDanger && tempElevated)) {
        a.level = LEVEL_EMERGENCY;
    }
    return a;
}

bool Monitor::update(const Inputs &in, const Thresholds &th, uint32_t nowMs) {
    const Assessment a = assess(in, th);
    const Level prev = level_;
    if (a.level >= level_) {
        level_ = a.level;
        reasons_ = a.reasons;
        lowerPending_ = false;
    } else {
        // Muốn hạ mức: phải ổn định đủ thời gian
        if (!lowerPending_) {
            lowerPending_ = true;
            lowerSinceMs_ = nowMs;
        } else if (nowMs - lowerSinceMs_ >= holdDownMs_) {
            level_ = a.level;
            reasons_ = a.reasons;
            lowerPending_ = false;
        }
    }
    return level_ != prev;
}

ChargeSession::Event ChargeSession::update(float currentA, float voltageV, uint32_t nowMs) {
    Event ev = EV_NONE;
    const bool flowing = !std::isnan(currentA) && currentA >= kChargingThresholdA;

    if (charging_ && !std::isnan(currentA)) {
        const uint32_t dt = nowMs - lastMs_;
        if (!std::isnan(voltageV) && dt < 60000) {
            energyWh_ += voltageV * currentA * (dt / 3600000.0f);
        }
    }
    lastMs_ = nowMs;

    if (!charging_) {
        if (flowing) {
            charging_ = true;
            startMs_ = nowMs;
            energyWh_ = 0.0f;
            lowPending_ = false;
            ev = EV_STARTED;
        }
    } else if (flowing) {
        lowPending_ = false;
    } else {
        if (!lowPending_) {
            lowPending_ = true;
            lowSinceMs_ = nowMs;
        } else if (nowMs - lowSinceMs_ >= kStopConfirmMs) {
            charging_ = false;
            lowPending_ = false;
            lastDurationMs_ = lowSinceMs_ - startMs_;
            if (lastDurationMs_ >= kMinSessionMs) ev = EV_FINISHED;
        }
    }
    return ev;
}

float RiseRate::update(float tempC, uint32_t nowMs) {
    if (std::isnan(tempC)) {
        valid_ = false;
        return NAN;
    }
    if (!valid_) {
        valid_ = true;
        lastT_ = tempC;
        lastMs_ = nowMs;
        rate_ = 0.0f;
        return rate_;
    }
    const uint32_t dt = nowMs - lastMs_;
    if (dt < 5000) return rate_;  // lấy mẫu tốc độ mỗi >= 5 giây
    const float inst = (tempC - lastT_) / (dt / 60000.0f);
    // EMA ~ 30 giây
    const float alpha = dt / (dt + 30000.0f);
    rate_ += alpha * (inst - rate_);
    lastT_ = tempC;
    lastMs_ = nowMs;
    return rate_;
}

float SmokeBaseline::update(float sensorMv, uint32_t nowMs) {
    if (std::isnan(sensorMv)) return NAN;
    if (!started_) {
        started_ = true;
        startMs_ = nowMs;
        baseline_ = sensorMv;
    }
    if (!warmed_) {
        // Trong lúc làm nóng: nền đi theo giá trị thực
        baseline_ += 0.2f * (sensorMv - baseline_);
        if (nowMs - startMs_ >= kWarmupMs) warmed_ = true;
        return NAN;
    }
    if (baseline_ < 1.0f) baseline_ = 1.0f;
    const float pct = (sensorMv - baseline_) * 100.0f / baseline_;
    if (sensorMv < baseline_) {
        baseline_ += 0.05f * (sensorMv - baseline_);  // không khí sạch hơn: hạ nền nhanh
    } else if (pct < 10.0f) {
        baseline_ += 0.002f * (sensorMv - baseline_);  // trôi nhiệt chậm: tăng nền rất chậm
    }
    return pct > 0.0f ? pct : 0.0f;
}

AirLevel airLevel(float co2ppm) {
    if (std::isnan(co2ppm) || co2ppm <= 0) return AIR_UNKNOWN;
    if (co2ppm < 800) return AIR_GOOD;
    if (co2ppm < 1000) return AIR_OK;
    if (co2ppm < 1500) return AIR_STUFFY;
    return AIR_BAD;
}

const char *levelNameVi(Level lv) {
    switch (lv) {
        case LEVEL_SAFE: return "An toàn";
        case LEVEL_WATCH: return "Theo dõi";
        case LEVEL_DANGER: return "Nguy cơ";
        case LEVEL_EMERGENCY: return "KHẨN CẤP";
    }
    return "?";
}

const char *levelNameAscii(Level lv) {
    switch (lv) {
        case LEVEL_SAFE: return "AN TOAN";
        case LEVEL_WATCH: return "THEO DOI";
        case LEVEL_DANGER: return "NGUY CO";
        case LEVEL_EMERGENCY: return "KHAN CAP";
    }
    return "?";
}

const char *airNameVi(AirLevel a) {
    switch (a) {
        case AIR_GOOD: return "Không khí tốt";
        case AIR_OK: return "Bình thường";
        case AIR_STUFFY: return "Hơi bí, nên mở cửa";
        case AIR_BAD: return "Kém! Mở cửa ngay";
        default: return "Chưa có số liệu";
    }
}

const char *airNameAscii(AirLevel a) {
    switch (a) {
        case AIR_GOOD: return "Khong khi tot";
        case AIR_OK: return "Binh thuong";
        case AIR_STUFFY: return "Hoi bi, nen mo cua";
        case AIR_BAD: return "Kem! Mo cua ngay";
        default: return "Chua co so lieu";
    }
}

const char *describeReasons(uint16_t reasons, char *buf, unsigned bufLen) {
    if (!buf || bufLen == 0) return "";
    buf[0] = '\0';
    struct Item {
        uint16_t bit;
        const char *text;
    };
    static const Item kItems[] = {
        {R_TEMP_HIGH, "nhiệt độ cao"},       {R_TEMP_RISE, "nhiệt tăng nhanh"},
        {R_SMOKE, "phát hiện khói/khí"},     {R_OVERCURRENT, "dòng sạc vượt ngưỡng"},
        {R_OVERVOLTAGE, "điện áp vượt ngưỡng"}, {R_SENSOR_FAULT, "mất cảm biến"},
    };
    unsigned used = 0;
    for (const Item &it : kItems) {
        if (!(reasons & it.bit)) continue;
        const int n = snprintf(buf + used, bufLen - used, "%s%s", used ? ", " : "", it.text);
        if (n < 0 || used + n >= bufLen) break;
        used += n;
    }
    if (used == 0) snprintf(buf, bufLen, "bình thường");
    return buf;
}

}  // namespace safety
