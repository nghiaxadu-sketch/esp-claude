// Driver tối giản cho các cảm biến (tự viết để không phụ thuộc thư viện ngoài):
//  - SCD41 (CO2, nhiệt độ, độ ẩm phòng) - I2C 0x62
//  - MLX90614 (nhiệt độ hồng ngoại không tiếp xúc, chĩa vào cục sạc/pin) - I2C 0x5A
//  - INA226 (<=36V) / INA228 (<=85V) (điện áp + dòng sạc DC) - I2C 0x40..0x4F
//  - MQ-2 (khói/khí gas) - ADC GPIO35
// Cảm biến nào không cắm thì tự bỏ qua.
#pragma once
#include <Arduino.h>

struct SensorReadings {
    float co2 = NAN;
    float roomTempC = NAN;
    float humidity = NAN;
    float chargerTempC = NAN;
    float voltageV = NAN;
    float currentA = NAN;
    float mq2Mv = NAN;
};

class SensorHub {
public:
    void begin(float shuntOhm);
    void setShunt(float ohm) { shuntOhm_ = ohm; }
    // Gọi định kỳ; cập nhật các giá trị mới nhất (giữ giá trị SCD41 cũ giữa 2 lần đo 5 giây)
    void poll(SensorReadings &out);
    bool hasScd41() const { return scd41_; }
    bool hasMlx() const { return mlx_; }
    bool hasIna() const { return inaAddr_ != 0; }
    bool isIna228() const { return ina228_; }
    bool hasMq2() const { return mq2_; }
    // Thử dò lại cảm biến bị mất (gọi thưa, ví dụ mỗi 30 giây)
    void rescan();

private:
    bool probe(uint8_t addr);
    bool scdCommand(uint16_t cmd);
    bool scdReadWords(uint16_t cmd, uint16_t *words, size_t n);
    bool scdStart();
    bool readScd();
    bool readMlx(float &tempC);
    bool detectIna();
    bool readIna(float &v, float &a);
    bool inaRead16(uint8_t reg, uint16_t &val);
    bool inaRead24(uint8_t reg, uint32_t &val);
    bool inaWrite16(uint8_t reg, uint16_t val);
    float readMq2Mv();

    float shuntOhm_ = 0.01f;
    bool scd41_ = false;
    bool mlx_ = false;
    uint8_t inaAddr_ = 0;
    bool ina228_ = false;
    bool mq2_ = false;
    uint8_t scdFails_ = 0;
    uint8_t mlxFails_ = 0;
    uint8_t inaFails_ = 0;
    float lastCo2_ = NAN, lastRoomT_ = NAN, lastHum_ = NAN;
    uint32_t lastScdMs_ = 0;
};
