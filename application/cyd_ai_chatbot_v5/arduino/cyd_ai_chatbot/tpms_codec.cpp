#include "tpms_codec.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace tpms {
namespace {

constexpr size_t kFrameLength = 18;
constexpr uint8_t kCompany0 = 0x00;
constexpr uint8_t kCompany1 = 0x01;
constexpr uint8_t kWheelBase = 0x80;
constexpr uint8_t kWheelCount = 4;
constexpr float kPaPerKpa = 1000.0f;
constexpr float kKpaPerBar = 100.0f;
constexpr float kKpaPerPsi = 6.894757f;
constexpr float kMaxPlausibleKpa = 1000.0f;

const char *const kSlotNames[kSlotCount] = {"1. Truoc trai", "2. Truoc phai", "3. Sau trai", "4. Sau phai",
                                            "Lop du phong"};
const char *const kSlotShort[kSlotCount] = {"TT", "TP", "ST", "SP", "DP"};

uint32_t le32(const uint8_t *d)
{
    return (uint32_t)d[0] | (uint32_t)d[1] << 8 | (uint32_t)d[2] << 16 | (uint32_t)d[3] << 24;
}

}  // namespace

bool decode(const uint8_t *d, size_t len, Reading &out)
{
    if (len != kFrameLength || d[0] != kCompany0 || d[1] != kCompany1) {
        return false;
    }
    const float kpa = le32(d + 8) / kPaPerKpa;
    if (kpa > kMaxPlausibleKpa) {
        return false;
    }
    snprintf(out.id, sizeof(out.id), "%02X%02X%02X%02X%02X%02X", d[2], d[3], d[4], d[5], d[6], d[7]);
    out.wheelHint = d[2] >= kWheelBase && d[2] < kWheelBase + kWheelCount ? (int8_t)(d[2] - kWheelBase) : -1;
    out.pressureKpa = kpa;
    out.temperatureC = (int32_t)le32(d + 12) / 100.0f;
    out.batteryPercent = d[16];
    out.alarm = d[17] != 0;
    return true;
}

Tyres::Tyres()
{
    memset(slots_, 0, sizeof(slots_));
}

int Tyres::feed(const Reading &r, uint32_t nowMs)
{
    int index = -1;
    for (size_t i = 0; i < kSlotCount; ++i) {
        if (slots_[i].id[0] != '\0' && strcmp(slots_[i].id, r.id) == 0) {
            index = (int)i;
        }
    }
    if (index < 0 && learning_ >= 0) {
        index = learning_;
        learning_ = -1;
        assign((size_t)index, r.id);
    }
    if (index < 0 && r.wheelHint >= 0 && slots_[r.wheelHint].id[0] == '\0') {
        index = r.wheelHint;
        assign((size_t)index, r.id);
    }
    if (index < 0) {
        return -1;
    }
    Slot &s = slots_[index];
    s.hasReading = true;
    s.last = r;
    s.lastMs = nowMs;
    return index;
}

void Tyres::assign(size_t slot, const char *id)
{
    if (slot >= kSlotCount) {
        return;
    }
    for (size_t i = 0; i < kSlotCount; ++i) {  // a sensor belongs to one tyre only
        if (i != slot && strcmp(slots_[i].id, id) == 0) {
            forget(i);
        }
    }
    Slot &s = slots_[slot];
    if (strcmp(s.id, id) != 0) {
        s.hasReading = false;
    }
    snprintf(s.id, sizeof(s.id), "%s", id);
}

void Tyres::forget(size_t slot)
{
    if (slot < kSlotCount) {
        memset(&slots_[slot], 0, sizeof(Slot));
    }
}

void Tyres::swap(size_t a, size_t b)
{
    if (a < kSlotCount && b < kSlotCount && a != b) {
        const Slot tmp = slots_[a];
        slots_[a] = slots_[b];
        slots_[b] = tmp;
    }
}

Status Tyres::status(size_t i, const Limits &limits, uint32_t nowMs) const
{
    const Slot &s = slots_[i];
    if (s.id[0] == '\0') {
        return Status::NotPaired;
    }
    if (!s.hasReading) {
        return Status::NoData;
    }
    if (nowMs - s.lastMs > kLostAfterMs) {
        return Status::Lost;
    }
    if (s.last.alarm) {
        return Status::Alarm;
    }
    if (limits.alertHigh && s.last.pressureKpa > limits.highKpa) {
        return Status::High;
    }
    if (limits.alertLow && s.last.pressureKpa < limits.lowKpa) {
        return Status::Low;
    }
    if (s.last.batteryPercent <= limits.lowBatteryPercent) {
        return Status::LowBattery;
    }
    return Status::Normal;
}

bool Tyres::anyAlert(const Limits &limits, uint32_t nowMs) const
{
    for (size_t i = 0; i < kSlotCount; ++i) {
        if (statusIsAlert(status(i, limits, nowMs))) {
            return true;
        }
    }
    return false;
}

const char *slotName(size_t slot)
{
    return slot < kSlotCount ? kSlotNames[slot] : "";
}

const char *slotShort(size_t slot)
{
    return slot < kSlotCount ? kSlotShort[slot] : "";
}

const char *statusText(Status status)
{
    switch (status) {
    case Status::NotPaired: return "(chua ghep)";
    case Status::NoData: return "Chua nhan duoc goi nao";
    case Status::Lost: return "Mat song";
    case Status::High: return "Ap cao";
    case Status::Low: return "Ap thap";
    case Status::Alarm: return "Bao dong";
    case Status::LowBattery: return "Pin yeu";
    case Status::Normal: return "Binh thuong";
    }
    return "";
}

bool statusIsAlert(Status status)
{
    return status == Status::High || status == Status::Low || status == Status::Alarm || status == Status::Lost;
}

const char *pressureUnitName(PressureUnit unit)
{
    switch (unit) {
    case PressureUnit::Psi: return "psi";
    case PressureUnit::Kpa: return "kPa";
    case PressureUnit::Bar:
    default: return "bar";
    }
}

void formatPressure(float kpa, PressureUnit unit, char *buf, size_t size)
{
    switch (unit) {
    case PressureUnit::Psi: snprintf(buf, size, "%.1f psi", kpa / kKpaPerPsi); break;
    case PressureUnit::Kpa: snprintf(buf, size, "%d kPa", (int)lroundf(kpa)); break;
    case PressureUnit::Bar:
    default: snprintf(buf, size, "%.2f bar", kpa / kKpaPerBar); break;
    }
}

void formatTemperature(float celsius, TempUnit unit, char *buf, size_t size)
{
    if (unit == TempUnit::Fahrenheit) {
        snprintf(buf, size, "%d F", (int)lroundf(celsius * 9 / 5 + 32));
    } else {
        snprintf(buf, size, "%d C", (int)lroundf(celsius));
    }
}

void formatAgo(uint32_t ms, char *buf, size_t size)
{
    const uint32_t s = ms / 1000;
    if (s < 60) {
        snprintf(buf, size, "%u giay truoc", (unsigned)s);
    } else if (s < 3600) {
        snprintf(buf, size, "%u phut truoc", (unsigned)(s / 60));
    } else {
        snprintf(buf, size, "%u gio truoc", (unsigned)(s / 3600));
    }
}

Reading demoReading(size_t slot, uint32_t nowMs)
{
    static const float kBase[kSlotCount] = {235, 238, 228, 172, 250};  // rear right is low on purpose
    Reading r = {};
    snprintf(r.id, sizeof(r.id), "DEMO0000000%u", (unsigned)(slot % 10));
    r.wheelHint = slot < kWheelCount ? (int8_t)slot : -1;
    const float t = nowMs / 1000.0f;
    r.pressureKpa = kBase[slot % kSlotCount] + 3 * sinf(t / 20 + (float)slot);
    r.temperatureC = 31 + 4 * sinf(t / 60 + (float)slot);
    r.batteryPercent = slot == 4 ? 12 : 80;
    r.alarm = false;
    return r;
}

}  // namespace tpms
