// PC test for the ELM327 / OBD-II decoder: sensor formulas, CAN multi-frame replies, fault codes and VIN.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "obd_codec.h"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        ++failures;
    }
}

bool near(float a, float b)
{
    return fabsf(a - b) < 0.01f;
}

float pidValue(const char *reply, uint8_t pid)
{
    uint8_t data[obd::kMaxReplyBytes];
    size_t len = 0;
    float v = -999;
    if (obd::parseMode01(reply, pid, data, len)) {
        obd::decodePid(pid, data, len, v);
    }
    return v;
}

}  // namespace

int main()
{
    // Mode 01 replies with and without spaces, prompt and SEARCHING line.
    check(near(pidValue("SEARCHING...\r410D32\r\r>", 0x0D), 50), "speed 50 km/h");
    check(near(pidValue("41 0C 1A F8\r>", 0x0C), 1726), "rpm 1726");
    check(near(pidValue("41057B", 0x05), 83), "coolant 83 C");
    check(near(pidValue("410F44", 0x0F), 28), "intake 28 C");
    check(near(pidValue("4111FF", 0x11), 100), "throttle 100%");
    check(near(pidValue("410E80", 0x0E), 0), "timing 0 deg");
    check(near(pidValue("4110 01 F4", 0x10), 5), "MAF 5 g/s");
    check(near(pidValue("41420D48", 0x42), 3.4f), "module voltage");
    check(near(pidValue("41A6000186A0", 0xA6), 10000), "odometer 10000 km");
    check(near(pidValue("410680", 0x06), 0), "fuel trim 0%");
    // Two ECUs answering: the first matching line wins.
    check(near(pidValue("410D14\r410D14\r", 0x0D), 20), "two ECUs");
    check(pidValue("NO DATA\r>", 0x0D) == -999, "no data");

    check(obd::replyError("NO DATA\r\r>") != nullptr, "NO DATA is an error");
    check(obd::replyError("UNABLE TO CONNECT") != nullptr, "UNABLE TO CONNECT is an error");
    check(obd::replyError("410D32") == nullptr, "valid reply is not an error");
    check(strcmp(obd::fuelSystemText(2), "Vong kin") == 0, "closed loop text");

    float volts = 0;
    check(obd::parseVoltage("12.6V\r>", volts) && near(volts, 12.6f), "ATRV 12.6V");
    check(!obd::parseVoltage("?", volts), "ATRV garbage");

    bool supported[256] = {};
    check(obd::parseSupported("4100BE3EB813", 0x00, supported), "supported bitmap parses");
    check(supported[0x01] && supported[0x0C] && supported[0x0D] && !supported[0x02], "supported PIDs");

    // Fault codes: CAN with count byte, multi-frame, older protocol with padding.
    obd::Dtc dtcs[8];
    size_t n = obd::parseDtcs("43020133017100\r>", obd::DtcKind::Stored, true, dtcs, 8);
    check(n == 2 && strcmp(dtcs[0].code, "P0133") == 0 && strcmp(dtcs[1].code, "P0171") == 0, "CAN DTCs");
    n = obd::parseDtcs("00A\r0:43040420C121\r1:D0010300000000\r", obd::DtcKind::Stored, true, dtcs, 8);
    check(n == 4 && strcmp(dtcs[0].code, "P0420") == 0 && strcmp(dtcs[1].code, "U0121") == 0 &&
              strcmp(dtcs[2].code, "U1001") == 0 && strcmp(dtcs[3].code, "P0300") == 0,
          "CAN multi-frame DTCs");
    n = obd::parseDtcs("43 01 33 00 00 00 00\r", obd::DtcKind::Stored, false, dtcs, 8);
    check(n == 1 && strcmp(dtcs[0].code, "P0133") == 0, "legacy DTC with padding");
    n = obd::parseDtcs("4700\r", obd::DtcKind::Pending, true, dtcs, 8);
    check(n == 0, "no pending codes");
    check(obd::dtcMode(obd::DtcKind::Permanent) == 0x0A, "permanent mode 0A");
    check(strcmp(obd::dtcSystem('C'), "Khung gam") == 0, "chassis");
    check(obd::dtcDescription("P0420") != nullptr && obd::dtcDescription("P9999") == nullptr, "descriptions");

    // VIN: CAN multi-frame and legacy multi-line.
    char vin[obd::kVinLength + 1];
    check(obd::parseVin("014\r0:490201314731\r1:4A433534343452\r2:37323532333637\r", vin) &&
              strcmp(vin, "1G1JC5444R7252367") == 0,
          "CAN VIN");
    check(obd::parseVin("49020100000031\r49020247314A43\r49020335343434\r49020452373235\r49020532333637\r", vin) &&
              strcmp(vin, "1G1JC5444R7252367") == 0,
          "legacy VIN");
    check(!obd::parseVin("NO DATA", vin), "no VIN");

    check(obd::protocolIsCan("A6") && obd::protocolIsCan("8") && !obd::protocolIsCan("A3") &&
              !obd::protocolIsCan("5"),
          "protocol is CAN");

    size_t count = 0;
    const obd::PidInfo *table = obd::pidTable(count);
    check(count >= 35 && table[0].pid == 0x0D, "PID table");
    for (size_t i = 0; i < count; ++i) {
        check(obd::findPid(table[i].pid) == &table[i], "findPid");
    }
    check(obd::demoValue(0x0C, 5000) >= 800, "demo rpm");

    if (failures == 0) {
        printf("obd_codec: all tests passed\n");
        return 0;
    }
    printf("obd_codec: %d failures\n", failures);
    return 1;
}
