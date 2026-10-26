#include "sensors.h"

#include <Wire.h>

#include "config.h"
#include "sensor_math.h"

namespace {

// SCD4x
constexpr uint16_t SCD_START_PERIODIC = 0x21B1;
constexpr uint16_t SCD_STOP_PERIODIC = 0x3F86;
constexpr uint16_t SCD_READ_MEASUREMENT = 0xEC05;
constexpr uint16_t SCD_DATA_READY = 0xE4B8;
constexpr uint32_t SCD_INTERVAL_MS = 5000;
// MLX90614
constexpr uint8_t MLX_REG_TOBJ1 = 0x07;
// INA226 / INA228
constexpr uint8_t INA226_REG_CONFIG = 0x00;
constexpr uint8_t INA226_REG_SHUNT = 0x01;
constexpr uint8_t INA226_REG_BUS = 0x02;
constexpr uint8_t INA226_REG_MFR = 0xFE;
constexpr uint8_t INA226_REG_DIE = 0xFF;
constexpr uint16_t INA226_DIE_ID = 0x2260;
constexpr uint16_t INA226_CONFIG_AVG16_1MS = 0x4527;  // AVG=16, VBUSCT=VSHCT=1.1ms, liên tục
constexpr uint8_t INA228_REG_ADC_CONFIG = 0x01;
constexpr uint8_t INA228_REG_VSHUNT = 0x04;
constexpr uint8_t INA228_REG_VBUS = 0x05;
constexpr uint8_t INA228_REG_MFR = 0x3E;
constexpr uint8_t INA228_REG_DEVICE = 0x3F;
constexpr uint16_t INA228_DEVICE_ID = 0x228;            // 12 bit cao của DEVICE_ID
constexpr uint16_t INA228_ADC_CONT_1MS_AVG16 = 0xFB6A;  // MODE=0xF, 1052µs, AVG=16
constexpr uint16_t TI_MANUFACTURER_ID = 0x5449;         // "TI"
constexpr uint8_t MAX_FAILS = 5;

}  // namespace

void SensorHub::begin(float shuntOhm) {
    shuntOhm_ = shuntOhm;
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
    Wire.setTimeOut(50);
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_MQ2_ADC, ADC_11db);
    rescan();
}

void SensorHub::rescan() {
    if (!scd41_ && probe(I2C_ADDR_SCD41)) {
        scd41_ = scdStart();
        scdFails_ = 0;
    }
    if (!mlx_ && probe(I2C_ADDR_MLX90614)) {
        float t;
        mlx_ = readMlx(t);
        mlxFails_ = 0;
    }
    if (inaAddr_ == 0) detectIna();
    if (!mq2_) {
        // Có MQ-2 khi điện áp đo được rõ ràng > 0 (cầu chia áp có R2 xuống GND nên khi rút cảm biến ra sẽ đọc ~0)
        const float mv = readMq2Mv();
        mq2_ = mv > 80.0f;
    }
}

bool SensorHub::probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// ---------------- SCD41 ----------------

bool SensorHub::scdCommand(uint16_t cmd) {
    Wire.beginTransmission(I2C_ADDR_SCD41);
    Wire.write(static_cast<uint8_t>(cmd >> 8));
    Wire.write(static_cast<uint8_t>(cmd & 0xFF));
    return Wire.endTransmission() == 0;
}

bool SensorHub::scdReadWords(uint16_t cmd, uint16_t *words, size_t n) {
    if (!scdCommand(cmd)) return false;
    delay(2);
    const size_t bytes = n * 3;
    if (Wire.requestFrom(static_cast<uint8_t>(I2C_ADDR_SCD41), static_cast<uint8_t>(bytes)) != bytes) return false;
    for (size_t i = 0; i < n; ++i) {
        uint8_t b[3];
        for (int k = 0; k < 3; ++k) b[k] = Wire.read();
        if (sensormath::sensirionCrc8(b, 2) != b[2]) return false;
        words[i] = static_cast<uint16_t>((b[0] << 8) | b[1]);
    }
    return true;
}

bool SensorHub::scdStart() {
    // Dừng phép đo cũ (nếu ESP32 vừa reset mà SCD41 vẫn đang đo) rồi bắt đầu lại
    scdCommand(SCD_STOP_PERIODIC);
    delay(500);
    return scdCommand(SCD_START_PERIODIC);
}

bool SensorHub::readScd() {
    uint16_t ready = 0;
    if (!scdReadWords(SCD_DATA_READY, &ready, 1)) return false;
    if ((ready & 0x07FF) == 0) return true;  // chưa có số liệu mới: không phải lỗi
    uint16_t w[3];
    if (!scdReadWords(SCD_READ_MEASUREMENT, w, 3)) return false;
    if (w[0] == 0) return true;  // giá trị 0 = phép đo chưa hợp lệ (vài giây đầu)
    lastCo2_ = w[0];
    lastRoomT_ = sensormath::scd4xTemperatureC(w[1]);
    lastHum_ = sensormath::scd4xHumidityPct(w[2]);
    return true;
}

// ---------------- MLX90614 ----------------

bool SensorHub::readMlx(float &tempC) {
    Wire.beginTransmission(I2C_ADDR_MLX90614);
    Wire.write(MLX_REG_TOBJ1);
    if (Wire.endTransmission(false) != 0) return false;  // repeated start
    if (Wire.requestFrom(static_cast<uint8_t>(I2C_ADDR_MLX90614), static_cast<uint8_t>(3)) != 3) return false;
    const uint8_t lo = Wire.read();
    const uint8_t hi = Wire.read();
    const uint8_t pec = Wire.read();
    const uint8_t frame[5] = {static_cast<uint8_t>(I2C_ADDR_MLX90614 << 1), MLX_REG_TOBJ1,
                              static_cast<uint8_t>((I2C_ADDR_MLX90614 << 1) | 1), lo, hi};
    if (sensormath::smbusPec(frame, sizeof(frame)) != pec) return false;
    if (hi & 0x80) return false;  // cờ lỗi
    tempC = sensormath::mlxRawToC(static_cast<uint16_t>((hi << 8) | lo));
    return tempC > -40.0f && tempC < 380.0f;
}

// ---------------- INA226 / INA228 ----------------

bool SensorHub::inaRead16(uint8_t reg, uint16_t &val) {
    Wire.beginTransmission(inaAddr_);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(inaAddr_, static_cast<uint8_t>(2)) != 2) return false;
    val = static_cast<uint16_t>(Wire.read() << 8);
    val |= Wire.read();
    return true;
}

bool SensorHub::inaRead24(uint8_t reg, uint32_t &val) {
    Wire.beginTransmission(inaAddr_);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(inaAddr_, static_cast<uint8_t>(3)) != 3) return false;
    val = static_cast<uint32_t>(Wire.read()) << 16;
    val |= static_cast<uint32_t>(Wire.read()) << 8;
    val |= Wire.read();
    return true;
}

bool SensorHub::inaWrite16(uint8_t reg, uint16_t val) {
    Wire.beginTransmission(inaAddr_);
    Wire.write(reg);
    Wire.write(static_cast<uint8_t>(val >> 8));
    Wire.write(static_cast<uint8_t>(val & 0xFF));
    return Wire.endTransmission() == 0;
}

bool SensorHub::detectIna() {
    for (uint8_t addr = I2C_ADDR_INA_FIRST; addr <= I2C_ADDR_INA_LAST; ++addr) {
        if (!probe(addr)) continue;
        inaAddr_ = addr;
        uint16_t mfr = 0, id = 0;
        if (inaRead16(INA228_REG_MFR, mfr) && mfr == TI_MANUFACTURER_ID && inaRead16(INA228_REG_DEVICE, id) &&
            (id >> 4) == INA228_DEVICE_ID) {
            ina228_ = true;
            if (inaWrite16(INA228_REG_ADC_CONFIG, INA228_ADC_CONT_1MS_AVG16)) {
                inaFails_ = 0;
                return true;
            }
        }
        if (inaRead16(INA226_REG_MFR, mfr) && mfr == TI_MANUFACTURER_ID && inaRead16(INA226_REG_DIE, id) &&
            id == INA226_DIE_ID) {
            ina228_ = false;
            if (inaWrite16(INA226_REG_CONFIG, INA226_CONFIG_AVG16_1MS)) {
                inaFails_ = 0;
                return true;
            }
        }
    }
    inaAddr_ = 0;
    return false;
}

bool SensorHub::readIna(float &v, float &a) {
    if (ina228_) {
        uint32_t sh = 0, bus = 0;
        if (!inaRead24(INA228_REG_VSHUNT, sh) || !inaRead24(INA228_REG_VBUS, bus)) return false;
        v = sensormath::ina228BusV(bus);
        a = sensormath::ina228ShuntMv(sh) / 1000.0f / shuntOhm_;
    } else {
        uint16_t sh = 0, bus = 0;
        if (!inaRead16(INA226_REG_SHUNT, sh) || !inaRead16(INA226_REG_BUS, bus)) return false;
        v = sensormath::ina226BusV(bus);
        a = sensormath::ina226ShuntMv(static_cast<int16_t>(sh)) / 1000.0f / shuntOhm_;
    }
    if (fabsf(a) < 0.02f) a = 0.0f;  // khử nhiễu quanh 0
    a = fabsf(a);                    // lắp shunt ngược chiều vẫn đọc đúng độ lớn
    return true;
}

// ---------------- MQ-2 ----------------

float SensorHub::readMq2Mv() {
    uint32_t sum = 0;
    constexpr int kSamples = 16;
    for (int i = 0; i < kSamples; ++i) sum += analogReadMilliVolts(PIN_MQ2_ADC);
    return (sum / static_cast<float>(kSamples)) * MQ2_DIVIDER_RATIO;
}

// ---------------- Tổng hợp ----------------

void SensorHub::poll(SensorReadings &out) {
    const uint32_t now = millis();

    if (scd41_ && now - lastScdMs_ >= SCD_INTERVAL_MS) {
        lastScdMs_ = now;
        if (readScd()) {
            scdFails_ = 0;
        } else if (++scdFails_ >= MAX_FAILS) {
            scd41_ = false;
            lastCo2_ = lastRoomT_ = lastHum_ = NAN;
        }
    }
    out.co2 = lastCo2_;
    out.roomTempC = lastRoomT_;
    out.humidity = lastHum_;

    out.chargerTempC = NAN;
    if (mlx_) {
        float t;
        if (readMlx(t)) {
            out.chargerTempC = t;
            mlxFails_ = 0;
        } else if (++mlxFails_ >= MAX_FAILS) {
            mlx_ = false;
        }
    }

    out.voltageV = out.currentA = NAN;
    if (inaAddr_) {
        float v, a;
        if (readIna(v, a)) {
            out.voltageV = v;
            out.currentA = a;
            inaFails_ = 0;
        } else if (++inaFails_ >= MAX_FAILS) {
            inaAddr_ = 0;
        }
    }

    out.mq2Mv = mq2_ ? readMq2Mv() : NAN;
    if (mq2_ && out.mq2Mv < 30.0f) mq2_ = false;  // rút cảm biến ra
}
