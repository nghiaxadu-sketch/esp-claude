// Trạng thái dùng chung giữa các task (cảm biến, màn hình, Telegram, AI, web).
// Mọi truy cập phải qua StateLock để tránh tranh chấp giữa 2 nhân CPU.
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"
#include "safety_logic.h"

struct SensorPresence {
    bool scd41 = false;
    bool mlx = false;
    bool ina = false;
    bool ina228 = false;  // false = INA226
    bool mq2 = false;
};

struct Snapshot {
    // Không khí (SCD41)
    float co2 = NAN;
    float roomTempC = NAN;
    float humidity = NAN;
    // Sạc xe
    float chargerTempC = NAN;  // MLX90614 (nhiệt độ bề mặt cục sạc/pin)
    float riseCPerMin = NAN;
    float smokePct = NAN;      // NAN khi MQ-2 đang làm nóng
    float smokeMv = NAN;
    float voltageV = NAN;
    float currentA = NAN;
    bool charging = false;
    uint32_t chargeMs = 0;
    float chargeWh = 0;
    // Đánh giá
    safety::Level level = safety::LEVEL_SAFE;
    uint16_t reasons = 0;
    safety::AirLevel air = safety::AIR_UNKNOWN;
    bool silenced = false;
    uint32_t silenceUntilMs = 0;
    SensorPresence present;
    uint32_t updatedMs = 0;
};

struct History {
    int16_t co2[HISTORY_LEN];          // ppm, -1 = không có
    int16_t chargerTempX10[HISTORY_LEN];  // 0.1°C, INT16_MIN = không có
    uint16_t head = 0;   // vị trí ghi tiếp theo
    uint16_t count = 0;
};

// Thông tin hiển thị trang AI trên TFT
struct AiView {
    char lastQuestion[160] = "";
    char lastAnswer[700] = "";  // ASCII (đã bỏ dấu)
    bool busy = false;
};

struct DeviceInfo {
    char ip[16] = "";
    char pin[7] = "";           // mã PIN ghép đôi Telegram + mật khẩu trang web
    bool wifiOk = false;
    bool telegramOk = false;
    bool aiConfigured = false;
    bool portalActive = false;
    char portalSsid[24] = "";
    char portalPass[12] = "";
};

class AppState {
public:
    static void begin();
    static void lock();
    static void unlock();

    // Các biến dưới đây CHỈ truy cập khi đang giữ khóa
    static Snapshot snap;
    static safety::Thresholds th;
    static History hist;
    static AiView ai;
    static DeviceInfo dev;

    static void pushHistory(float co2, float chargerTempC);

private:
    static SemaphoreHandle_t mutex_;
};

// Khóa kiểu RAII: { StateLock l; ...đọc/ghi AppState... }
class StateLock {
public:
    StateLock() { AppState::lock(); }
    ~StateLock() { AppState::unlock(); }
    StateLock(const StateLock &) = delete;
    StateLock &operator=(const StateLock &) = delete;
};
