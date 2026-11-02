// Bluetooth tyre pressure sensors (TPMS): decoding their advertisements, the 5 tyre slots (4 wheels + spare),
// alerts (high / low pressure, sensor alarm, low battery, lost signal) and unit formatting.
//
// Supported advertisement: the widely sold "TPMS" BLE valve-cap / internal sensors whose 18-byte
// manufacturer data is
//   [0..1] 00 01 | [2..7] sensor address (byte 2 = 0x80 + wheel number) | [8..11] pressure, Pa, little endian
//   [12..15] temperature, 1/100 C, little endian | [16] battery % | [17] alarm flag.
// Other brands (Vietmap BT10, Bekubee, Sysgration, Pecham, Wicarlink) use their own formats that are not
// published; they are listed by name in the scan so their raw data can be added later.
// Plain C++ (no Arduino headers) so it is unit-tested on a PC.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace tpms {

constexpr size_t kSlotCount = 5;  // front left, front right, rear left, rear right, spare
constexpr size_t kIdLength = 13;  // 12 hex digits + terminator
constexpr uint32_t kLostAfterMs = 10UL * 60 * 1000;  // sensors send about once a minute while driving

struct Reading {
    char id[kIdLength];   // sensor address, hex
    int8_t wheelHint;     // 0..3 from the address, -1 unknown
    float pressureKpa;    // gauge pressure
    float temperatureC;
    uint8_t batteryPercent;
    bool alarm;
};

// Decodes manufacturer data (as received, company id first). False for other devices.
bool decode(const uint8_t *data, size_t len, Reading &out);

enum class Status : uint8_t { NotPaired, NoData, Lost, High, Low, Alarm, LowBattery, Normal };
enum class PressureUnit : uint8_t { Bar, Psi, Kpa };
enum class TempUnit : uint8_t { Celsius, Fahrenheit };

struct Limits {
    float lowKpa = 180;   // 1.8 bar
    float highKpa = 300;  // 3.0 bar
    bool alertLow = true;
    bool alertHigh = true;
    uint8_t lowBatteryPercent = 15;
};

struct Slot {
    char id[kIdLength];  // empty = not paired
    bool hasReading;
    Reading last;
    uint32_t lastMs;     // millis() of the last packet
};

// The 5 tyre slots with pairing, swapping and status logic.
class Tyres {
public:
    Tyres();

    // Feeds a packet; returns the slot it belongs to (pairing it to the slot waiting for a sensor, or to the
    // wheel number in its address when that slot is free), or -1 when it is not one of ours.
    int feed(const Reading &reading, uint32_t nowMs);
    // The next unknown sensor seen goes to this slot (-1 cancels).
    void learn(int slot) { learning_ = slot; }
    int learning() const { return learning_; }
    void swap(size_t a, size_t b);
    void forget(size_t slot);
    void assign(size_t slot, const char *id);

    const Slot &slot(size_t i) const { return slots_[i]; }
    Status status(size_t i, const Limits &limits, uint32_t nowMs) const;
    // True when any slot shows an alert (for the alarm sound).
    bool anyAlert(const Limits &limits, uint32_t nowMs) const;

private:
    Slot slots_[kSlotCount];
    int learning_ = -1;
};

const char *slotName(size_t slot);   // "1. Truoc trai" ...
const char *slotShort(size_t slot);  // "TT", "TP", "ST", "SP", "DP"
const char *statusText(Status status);
bool statusIsAlert(Status status);

// "2.35 bar", "34.1 psi", "235 kPa"; "28 C" / "82 F".
void formatPressure(float kpa, PressureUnit unit, char *buf, size_t size);
void formatTemperature(float celsius, TempUnit unit, char *buf, size_t size);
// "12 giay truoc", "3 phut truoc", "2 gio truoc".
void formatAgo(uint32_t ms, char *buf, size_t size);
const char *pressureUnitName(PressureUnit unit);

// Five fake sensors that drift slowly (demo mode), one per slot.
Reading demoReading(size_t slot, uint32_t nowMs);

}  // namespace tpms
