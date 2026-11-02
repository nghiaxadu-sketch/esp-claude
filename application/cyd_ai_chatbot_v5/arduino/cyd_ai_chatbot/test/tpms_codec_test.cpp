// PC test for the TPMS decoder and tyre slot logic.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tpms_codec.h"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main()
{
    // 0x0003A1DE Pa = 238.046 kPa, 0x00000AE6 = 27.90 C, battery 91 %, no alarm, wheel 1 (front left).
    const uint8_t frame[18] = {0x00, 0x01, 0x80, 0xEA, 0xCA, 0x10, 0x8A, 0x78, 0xDE, 0xA1,
                               0x03, 0x00, 0xE6, 0x0A, 0x00, 0x00, 0x5B, 0x00};
    tpms::Reading r;
    check(tpms::decode(frame, sizeof(frame), r), "decode frame");
    check(strcmp(r.id, "80EACA108A78") == 0, "sensor id");
    check(r.wheelHint == 0, "wheel hint");
    check(fabsf(r.pressureKpa - 238.046f) < 0.01f, "pressure");
    check(fabsf(r.temperatureC - 27.9f) < 0.01f, "temperature");
    check(r.batteryPercent == 91 && !r.alarm, "battery and alarm");
    check(!tpms::decode(frame, 17, r), "wrong length rejected");
    uint8_t other[18];
    memcpy(other, frame, sizeof(other));
    other[0] = 0x4C;  // Apple
    check(!tpms::decode(other, sizeof(other), r), "other company rejected");

    tpms::Tyres tyres;
    tpms::Limits limits;
    tpms::decode(frame, sizeof(frame), r);
    check(tyres.feed(r, 1000) == 0, "auto pair by wheel hint");
    check(tyres.status(0, limits, 2000) == tpms::Status::Normal, "normal");
    check(tyres.status(1, limits, 2000) == tpms::Status::NotPaired, "not paired");
    check(tyres.status(0, limits, 1000 + tpms::kLostAfterMs + 1) == tpms::Status::Lost, "lost");

    // Learning: next unknown sensor goes to the spare.
    tpms::Reading spare = r;
    snprintf(spare.id, sizeof(spare.id), "%s", "112233445566");
    spare.wheelHint = -1;
    spare.pressureKpa = 150;
    check(tyres.feed(spare, 1000) == -1, "unknown sensor ignored");
    tyres.learn(4);
    check(tyres.feed(spare, 1000) == 4 && tyres.learning() == -1, "learned spare");
    check(tyres.status(4, limits, 1500) == tpms::Status::Low, "low pressure");
    limits.alertLow = false;
    check(tyres.status(4, limits, 1500) == tpms::Status::Normal, "low alert off");
    limits.alertLow = true;

    tyres.swap(0, 4);
    check(strcmp(tyres.slot(4).id, "80EACA108A78") == 0 && strcmp(tyres.slot(0).id, "112233445566") == 0, "swap");
    check(tyres.anyAlert(limits, 1500), "any alert");
    tyres.assign(1, "112233445566");
    check(tyres.slot(0).id[0] == '\0', "a sensor belongs to one tyre");

    char buf[24];
    tpms::formatPressure(238, tpms::PressureUnit::Bar, buf, sizeof(buf));
    check(strcmp(buf, "2.38 bar") == 0, "bar");
    tpms::formatPressure(238, tpms::PressureUnit::Psi, buf, sizeof(buf));
    check(strcmp(buf, "34.5 psi") == 0, "psi");
    tpms::formatPressure(238, tpms::PressureUnit::Kpa, buf, sizeof(buf));
    check(strcmp(buf, "238 kPa") == 0, "kPa");
    tpms::formatTemperature(30, tpms::TempUnit::Fahrenheit, buf, sizeof(buf));
    check(strcmp(buf, "86 F") == 0, "fahrenheit");
    tpms::formatAgo(125000, buf, sizeof(buf));
    check(strcmp(buf, "2 phut truoc") == 0, "ago");

    tpms::Reading demo = tpms::demoReading(3, 0);
    check(demo.pressureKpa < limits.lowKpa, "demo rear right is low");

    if (failures == 0) {
        printf("tpms_codec: all tests passed\n");
        return 0;
    }
    printf("tpms_codec: %d failures\n", failures);
    return 1;
}
