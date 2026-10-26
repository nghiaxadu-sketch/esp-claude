// Logic đánh giá mức nguy cơ khi sạc xe + chất lượng không khí.
// Thuần C++ (không phụ thuộc Arduino) để test được trên máy tính.
#pragma once
#include <stdint.h>

#include <cmath>

namespace safety {

// 4 mức cảnh báo như sơ đồ: An toàn / Theo dõi / Nguy cơ / Khẩn cấp
enum Level : uint8_t {
    LEVEL_SAFE = 0,
    LEVEL_WATCH = 1,
    LEVEL_DANGER = 2,
    LEVEL_EMERGENCY = 3,
};

// Lý do (bitmask) để hiển thị / gửi tin nhắn
enum Reason : uint16_t {
    R_NONE = 0,
    R_TEMP_HIGH = 1 << 0,     // nhiệt độ cục sạc/pin cao
    R_TEMP_RISE = 1 << 1,     // nhiệt độ tăng nhanh
    R_SMOKE = 1 << 2,         // có khói/khí gas
    R_OVERCURRENT = 1 << 3,   // dòng sạc vượt ngưỡng
    R_OVERVOLTAGE = 1 << 4,   // điện áp vượt ngưỡng
    R_SENSOR_FAULT = 1 << 5,  // cảm biến quan trọng bị mất kết nối khi đang sạc
};

struct Thresholds {
    float tempWarnC = 45.0f;
    float tempDangerC = 55.0f;
    float tempEmergencyC = 70.0f;
    float riseWarnCPerMin = 2.0f;  // tăng >= 2°C/phút -> theo dõi, gấp đôi -> nguy cơ
    float smokeWarnPct = 20.0f;    // % tăng so với nền không khí sạch
    float smokeDangerPct = 40.0f;
    float currentMaxA = 10.0f;     // 0 = tắt
    float voltageMaxV = 0.0f;      // 0 = tắt
    float co2AlertPpm = 1500.0f;   // báo điện thoại khi CO2 vượt mức này
};

// Đầu vào; NAN = không có cảm biến / chưa có số liệu
struct Inputs {
    float chargerTempC = NAN;
    float tempRiseCPerMin = NAN;
    float smokePct = NAN;
    float currentA = NAN;
    float voltageV = NAN;
    bool sensorFault = false;  // mất cảm biến nhiệt khi đang sạc
};

struct Assessment {
    Level level = LEVEL_SAFE;
    uint16_t reasons = R_NONE;
};

// Đánh giá tức thời (không có trễ/hysteresis)
Assessment assess(const Inputs &in, const Thresholds &th);

// Bộ giám sát có trễ: lên mức ngay lập tức, xuống mức chỉ khi điều kiện đã ổn định holdMs.
class Monitor {
public:
    explicit Monitor(uint32_t holdDownMs = 30000) : holdDownMs_(holdDownMs) {}
    // Trả về true nếu mức thay đổi so với lần trước
    bool update(const Inputs &in, const Thresholds &th, uint32_t nowMs);
    Level level() const { return level_; }
    uint16_t reasons() const { return reasons_; }

private:
    uint32_t holdDownMs_;
    Level level_ = LEVEL_SAFE;
    uint16_t reasons_ = R_NONE;
    uint32_t lowerSinceMs_ = 0;
    bool lowerPending_ = false;
};

// Theo dõi phiên sạc: phát hiện bắt đầu/kết thúc sạc, thời gian, điện năng.
class ChargeSession {
public:
    enum Event : uint8_t { EV_NONE = 0, EV_STARTED, EV_FINISHED };
    static constexpr float kChargingThresholdA = 0.15f;
    static constexpr uint32_t kStopConfirmMs = 60000;   // dòng ~0 liên tục 60 giây -> coi là xong
    static constexpr uint32_t kMinSessionMs = 120000;   // phiên < 2 phút thì không báo "sạc xong"

    Event update(float currentA, float voltageV, uint32_t nowMs);
    bool charging() const { return charging_; }
    uint32_t durationMs(uint32_t nowMs) const { return charging_ ? nowMs - startMs_ : lastDurationMs_; }
    float energyWh() const { return energyWh_; }

private:
    bool charging_ = false;
    uint32_t startMs_ = 0;
    uint32_t lastMs_ = 0;
    uint32_t lowSinceMs_ = 0;
    bool lowPending_ = false;
    uint32_t lastDurationMs_ = 0;
    float energyWh_ = 0.0f;
};

// Ước tính tốc độ tăng nhiệt (°C/phút) bằng EMA, chống nhiễu.
class RiseRate {
public:
    void reset() { valid_ = false; }
    float update(float tempC, uint32_t nowMs);
    float value() const { return valid_ ? rate_ : NAN; }

private:
    bool valid_ = false;
    float lastT_ = 0.0f;
    uint32_t lastMs_ = 0;
    float rate_ = 0.0f;
};

// Theo dõi nền (baseline) của cảm biến khói MQ-2 và tính % tăng so với nền.
class SmokeBaseline {
public:
    static constexpr uint32_t kWarmupMs = 180000;  // MQ-2 cần làm nóng ~3 phút sau khi cấp điện
    float update(float sensorMv, uint32_t nowMs);  // trả về % (NAN khi đang làm nóng)
    bool warmedUp() const { return warmed_; }
    float baselineMv() const { return baseline_; }

private:
    bool started_ = false;
    bool warmed_ = false;
    uint32_t startMs_ = 0;
    float baseline_ = 0.0f;
};

// Chất lượng không khí theo CO2
enum AirLevel : uint8_t { AIR_GOOD = 0, AIR_OK, AIR_STUFFY, AIR_BAD, AIR_UNKNOWN };
AirLevel airLevel(float co2ppm);

const char *levelNameVi(Level lv);       // có dấu (Telegram/web)
const char *levelNameAscii(Level lv);    // không dấu (TFT)
const char *airNameVi(AirLevel a);
const char *airNameAscii(AirLevel a);
// Ghi mô tả lý do (tiếng Việt có dấu) vào buf, trả về buf
const char *describeReasons(uint16_t reasons, char *buf, unsigned bufLen);

}  // namespace safety
