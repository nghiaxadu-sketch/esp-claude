// Công thức chuyển đổi / CRC của các cảm biến (thuần C++, test được trên máy tính).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sensormath {

// CRC-8 của Sensirion (SCD4x): đa thức 0x31, khởi tạo 0xFF
inline uint8_t sensirionCrc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : static_cast<uint8_t>(crc << 1);
    }
    return crc;
}

// CRC-8 SMBus PEC (MLX90614): đa thức 0x07, khởi tạo 0x00
inline uint8_t smbusPec(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
    }
    return crc;
}

// SCD41
inline float scd4xTemperatureC(uint16_t raw) { return -45.0f + 175.0f * raw / 65535.0f; }
inline float scd4xHumidityPct(uint16_t raw) { return 100.0f * raw / 65535.0f; }

// MLX90614: 0.02 K/LSB
inline float mlxRawToC(uint16_t raw) { return raw * 0.02f - 273.15f; }

// INA226: shunt 2.5 µV/LSB (có dấu), bus 1.25 mV/LSB
inline float ina226ShuntMv(int16_t raw) { return raw * 0.0025f; }
inline float ina226BusV(uint16_t raw) { return raw * 0.00125f; }

// INA228: thanh ghi 24 bit, dữ liệu 20 bit ở bit 23..4
// shunt 312.5 nV/LSB (ADCRANGE=0, có dấu), bus 195.3125 µV/LSB
inline int32_t ina228Signed20(uint32_t reg24) {
    int32_t v = static_cast<int32_t>(reg24 >> 4) & 0xFFFFF;
    if (v & 0x80000) v -= 0x100000;
    return v;
}
inline float ina228ShuntMv(uint32_t reg24) { return ina228Signed20(reg24) * 0.0003125f; }
inline float ina228BusV(uint32_t reg24) { return ((reg24 >> 4) & 0xFFFFF) * 0.0001953125f; }

}  // namespace sensormath
