// OBD-II over an ELM327 adapter: the text protocol (AT commands, hex replies, CAN multi-frame lines), the
// standard SAE J1979 mode 01 sensors with Vietnamese names, fault codes (modes 03 / 07 / 0A, clear with 04),
// the VIN (mode 09 PID 02) and a demo data generator. Plain C++, unit-tested on a PC; the Bluetooth / Wi-Fi
// transport lives in obd_link.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace obd {

constexpr size_t kMaxReplyBytes = 96;
constexpr size_t kVinLength = 17;
constexpr size_t kDtcCodeLength = 6;  // "P0420" + terminator
constexpr uint8_t kPidVoltageFromAtrv = 0xFF;  // pseudo PID: battery voltage read with ATRV

// Commands sent after connecting (echo off, no line feeds, no spaces, no headers, automatic protocol).
extern const char *const kInitCommands[];
extern const size_t kInitCommandCount;

struct PidInfo {
    uint8_t pid;
    const char *nameVi;  // ASCII Vietnamese, e.g. "Vong tua may"
    const char *nameEn;
    const char *unit;    // "km/h", "v/p", "C", "%", ...
    uint8_t bytes;       // data bytes after "41 <pid>"
    uint8_t decimals;    // digits shown after the point
};

// Every sensor shown in "Tat ca cam bien" (including the ATRV battery voltage), in display order.
const PidInfo *pidTable(size_t &count);
const PidInfo *findPid(uint8_t pid);

// Value of a mode 01 PID from its data bytes (A, B, ...). False for unknown PIDs or short data.
bool decodePid(uint8_t pid, const uint8_t *data, size_t len, float &value);
// PID 03 (fuel system status) first byte -> "Vong kin", "Vong ho (may nguoi)"...
const char *fuelSystemText(uint8_t status);

// A reply split into messages: one per ECU line, CAN multi-frame lines ("0:", "1:"...) joined together.
struct Message {
    uint8_t bytes[kMaxReplyBytes];
    size_t len;
};

// Error replies ("NO DATA", "UNABLE TO CONNECT", "CAN ERROR"...). Returns a short ASCII reason or nullptr.
const char *replyError(const char *reply);
// Parses up to maxMessages messages from a raw reply (">" prompt, "SEARCHING..." and echo are ignored).
size_t parseMessages(const char *reply, Message *messages, size_t maxMessages);

// Finds "41 <pid> A B ..." in a reply and copies the data bytes.
bool parseMode01(const char *reply, uint8_t pid, uint8_t *data, size_t &len);
// Bitmap reply to 01 00 / 01 20 / ...: marks supported PIDs base+1 .. base+32 in supported[256].
bool parseSupported(const char *reply, uint8_t base, bool (&supported)[256]);
// "12.6V" from ATRV.
bool parseVoltage(const char *reply, float &volts);

enum class DtcKind : uint8_t { Stored, Pending, Permanent };

struct Dtc {
    char code[kDtcCodeLength];
    DtcKind kind;
};

// Mode byte of the request for a kind: 0x03, 0x07, 0x0A.
uint8_t dtcMode(DtcKind kind);
// Decodes the fault codes of a mode 03/07/0A reply. `can` = the ECU uses CAN (ISO 15765), whose replies
// carry a count byte after the mode byte.
size_t parseDtcs(const char *reply, DtcKind kind, bool can, Dtc *out, size_t maxOut);
void formatDtc(uint8_t a, uint8_t b, char (&code)[kDtcCodeLength]);
// "Dong co / hop so", "Khung gam", "Than xe", "Mang giao tiep" for P / C / B / U.
const char *dtcSystem(char letter);
// Short Vietnamese description of common codes, nullptr when unknown.
const char *dtcDescription(const char *code);

// VIN from a 09 02 reply (CAN or older protocols); false when fewer than 17 characters came back.
bool parseVin(const char *reply, char (&vin)[kVinLength + 1]);

// ATDPN reply ("A6", "6"...) -> true for CAN protocols 6..9 (and A..C user CAN).
bool protocolIsCan(const char *atdpnReply);

// Sample data so the screens can be tried without a car ("Du lieu mau").
float demoValue(uint8_t pid, uint32_t ms);
extern const char *const kDemoDtcs[];
extern const size_t kDemoDtcCount;
extern const char *const kDemoVin;

}  // namespace obd
