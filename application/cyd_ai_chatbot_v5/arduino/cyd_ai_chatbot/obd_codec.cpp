#include "obd_codec.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace obd {

const char *const kInitCommands[] = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"};
const size_t kInitCommandCount = sizeof(kInitCommands) / sizeof(kInitCommands[0]);

const char *const kDemoDtcs[] = {"P0420", "P0128", "P0442"};
const size_t kDemoDtcCount = sizeof(kDemoDtcs) / sizeof(kDemoDtcs[0]);
const char *const kDemoVin = "DEMOVN12345678901";

namespace {

constexpr uint8_t kMode01Reply = 0x41;
constexpr uint8_t kMode09Reply = 0x49;
constexpr uint8_t kVinPid = 0x02;
constexpr uint8_t kReplyOffset = 0x40;  // reply mode = request mode + 0x40
constexpr size_t kMaxLine = 128;
constexpr float kPercent = 100.0f / 255.0f;

const PidInfo kPids[] = {
    {0x0D, "Toc do", "Speed", "km/h", 1, 0},
    {0x0C, "Vong tua may", "Engine RPM", "v/p", 2, 0},
    {0x11, "Buom ga", "Throttle", "%", 1, 0},
    {0x04, "Tai dong co", "Engine load", "%", 1, 0},
    {0x05, "Nhiet do nuoc", "Coolant temp", "C", 1, 0},
    {0x0F, "Nhiet do khi nap", "Intake air temp", "C", 1, 0},
    {0x2F, "Nhien lieu", "Fuel level", "%", 1, 0},
    {kPidVoltageFromAtrv, "Dien ap accu", "Battery voltage", "V", 0, 1},
    {0x0B, "Ap suat khi nap", "Intake pressure", "kPa", 1, 0},
    {0x10, "Luu luong khi", "Mass air flow", "g/s", 2, 1},
    {0x03, "He thong nhien lieu", "Fuel system", "", 2, 0},
    {0x06, "Bu nhien lieu ngan han", "Short fuel trim", "%", 1, 1},
    {0x07, "Bu nhien lieu dai han", "Long fuel trim", "%", 1, 1},
    {0x0E, "Goc danh lua som", "Timing advance", "do", 1, 1},
    {0x0A, "Ap suat nhien lieu", "Fuel pressure", "kPa", 1, 0},
    {0x14, "Cam bien O2 (B1S1)", "O2 sensor B1S1", "V", 2, 2},
    {0x46, "Nhiet do moi truong", "Ambient temp", "C", 1, 0},
    {0x5C, "Nhiet do dau", "Oil temp", "C", 1, 0},
    {0x1F, "Thoi gian no may", "Run time", "s", 2, 0},
    {0x21, "Quang duong den Check sang", "Distance with MIL", "km", 2, 0},
    {0x31, "Quang duong tu lan xoa loi", "Distance since clear", "km", 2, 0},
    {0x33, "Ap suat khi quyen", "Barometric pressure", "kPa", 1, 0},
    {0x43, "Tai tuyet doi", "Absolute load", "%", 2, 0},
    {0x44, "Ti le khi/nhien lieu", "Commanded AFR", "lambda", 2, 2},
    {0x45, "Buom ga tuong doi", "Relative throttle", "%", 1, 0},
    {0x47, "Buom ga tuyet doi B", "Absolute throttle B", "%", 1, 0},
    {0x49, "Ban dap ga D", "Accelerator pedal D", "%", 1, 0},
    {0x4A, "Ban dap ga E", "Accelerator pedal E", "%", 1, 0},
    {0x4C, "Buom ga theo lenh", "Commanded throttle", "%", 1, 0},
    {0x4D, "Thoi gian den Check sang", "Time with MIL", "phut", 2, 0},
    {0x4E, "Thoi gian tu lan xoa loi", "Time since clear", "phut", 2, 0},
    {0x5A, "Ban dap ga tuong doi", "Relative pedal", "%", 1, 0},
    {0x5E, "Muc tieu thu nhien lieu", "Fuel rate", "L/h", 2, 1},
    {0x61, "Mo-men yeu cau", "Demanded torque", "%", 1, 0},
    {0x62, "Mo-men thuc te", "Actual torque", "%", 1, 0},
    {0x63, "Mo-men tham chieu", "Reference torque", "Nm", 2, 0},
    {0x42, "Dien ap module", "Module voltage", "V", 2, 2},
    {0xA6, "Dong ho tong (ODO)", "Odometer", "km", 4, 1},
};

struct DtcText {
    const char *code;
    const char *text;
};

const DtcText kDtcTexts[] = {
    {"P0100", "Loi mach cam bien luu luong khi (MAF)"},
    {"P0101", "Cam bien MAF ngoai dai/hieu suat"},
    {"P0110", "Loi mach cam bien nhiet do khi nap"},
    {"P0115", "Loi mach cam bien nhiet do nuoc"},
    {"P0120", "Loi mach cam bien buom ga"},
    {"P0128", "Nhiet do nuoc thap hon nguong van hang nhiet"},
    {"P0130", "Loi mach cam bien oxy (bank 1)"},
    {"P0171", "Hon hop qua ngheo (bank 1)"},
    {"P0172", "Hon hop qua giau (bank 1)"},
    {"P0300", "Phat hien bo may nhieu xy-lanh"},
    {"P0301", "Bo may xy-lanh 1"},
    {"P0302", "Bo may xy-lanh 2"},
    {"P0303", "Bo may xy-lanh 3"},
    {"P0304", "Bo may xy-lanh 4"},
    {"P0420", "Hieu suat bo xuc tac thap (bank 1)"},
    {"P0440", "Loi he thong kiem soat hoi xang (EVAP)"},
    {"P0442", "Ro ri nho he thong EVAP"},
    {"P0455", "Ro ri lon he thong EVAP"},
    {"P0500", "Loi cam bien toc do xe"},
    {"P0505", "Loi he thong dieu khien khong tai"},
    {"P0700", "Loi he thong dieu khien hop so"},
};

struct ErrorText {
    const char *needle;  // compared without spaces, upper case
    const char *reason;
};

const ErrorText kErrors[] = {
    {"NODATA", "Xe khong tra loi (NO DATA)"},
    {"UNABLETOCONNECT", "Khong noi duoc ECU"},
    {"BUSINIT", "Loi khoi tao bus"},
    {"CANERROR", "Loi bus CAN"},
    {"BUSERROR", "Loi bus"},
    {"FBERROR", "Loi phan hoi bus"},
    {"DATAERROR", "Loi du lieu"},
    {"STOPPED", "Adapter dung giua chung"},
    {"BUFFERFULL", "Bo dem adapter day"},
    {"7F01", "ECU tu choi lenh"},
};

int hexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)toupper((unsigned char)c);
    return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

// Copies `line` without spaces, upper case.
void compact(const char *line, size_t len, char *out, size_t size)
{
    size_t n = 0;
    for (size_t i = 0; i < len && n + 1 < size; ++i) {
        if (!isspace((unsigned char)line[i])) {
            out[n++] = (char)toupper((unsigned char)line[i]);
        }
    }
    out[n] = '\0';
}

bool allHex(const char *s)
{
    if (*s == '\0') {
        return false;
    }
    for (; *s; ++s) {
        if (hexValue(*s) < 0) {
            return false;
        }
    }
    return true;
}

void appendHex(const char *s, Message &m)
{
    for (; s[0] && s[1] && m.len < kMaxReplyBytes; s += 2) {
        m.bytes[m.len++] = (uint8_t)(hexValue(s[0]) << 4 | hexValue(s[1]));
    }
}

uint16_t word(const uint8_t *d)
{
    return (uint16_t)(d[0] << 8 | d[1]);
}

}  // namespace

const PidInfo *pidTable(size_t &count)
{
    count = sizeof(kPids) / sizeof(kPids[0]);
    return kPids;
}

const PidInfo *findPid(uint8_t pid)
{
    for (const PidInfo &p : kPids) {
        if (p.pid == pid) {
            return &p;
        }
    }
    return nullptr;
}

bool decodePid(uint8_t pid, const uint8_t *d, size_t len, float &v)
{
    const PidInfo *info = findPid(pid);
    if (info == nullptr || pid == kPidVoltageFromAtrv || len < info->bytes) {
        return false;
    }
    const float a = d[0];
    switch (pid) {
    case 0x04: case 0x11: case 0x2F: case 0x45: case 0x47: case 0x49: case 0x4A: case 0x4C: case 0x5A:
        v = a * kPercent;
        return true;
    case 0x05: case 0x0F: case 0x46: case 0x5C:
        v = a - 40;
        return true;
    case 0x06: case 0x07:
        v = (a - 128) * 100.0f / 128.0f;
        return true;
    case 0x0A: v = a * 3; return true;
    case 0x0B: case 0x33: case 0x0D: v = a; return true;
    case 0x0C: v = word(d) / 4.0f; return true;
    case 0x0E: v = a / 2.0f - 64; return true;
    case 0x10: v = word(d) / 100.0f; return true;
    case 0x14: v = a / 200.0f; return true;
    case 0x03: v = a; return true;
    case 0x1F: case 0x21: case 0x31: case 0x4D: case 0x4E: case 0x63: v = word(d); return true;
    case 0x42: v = word(d) / 1000.0f; return true;
    case 0x43: v = word(d) * kPercent; return true;
    case 0x44: v = word(d) * 2.0f / 65536.0f; return true;
    case 0x5E: v = word(d) / 20.0f; return true;
    case 0x61: case 0x62: v = a - 125; return true;
    case 0xA6:
        v = (float)((uint32_t)d[0] << 24 | (uint32_t)d[1] << 16 | (uint32_t)d[2] << 8 | d[3]) / 10.0f;
        return true;
    default:
        return false;
    }
}

const char *fuelSystemText(uint8_t status)
{
    switch (status) {
    case 0: return "Tat";
    case 1: return "Vong ho (may nguoi)";
    case 2: return "Vong kin";
    case 4: return "Vong ho (tai/giam ga)";
    case 8: return "Vong ho (loi)";
    case 16: return "Vong kin (loi)";
    default: return "Khong xac dinh";
    }
}

const char *replyError(const char *reply)
{
    char line[kMaxLine];
    compact(reply, strlen(reply), line, sizeof(line));
    for (const ErrorText &e : kErrors) {
        if (strstr(line, e.needle) != nullptr) {
            return e.reason;
        }
    }
    if (strchr(line, '?') != nullptr) {
        return "Adapter khong hieu lenh";
    }
    return nullptr;
}

size_t parseMessages(const char *reply, Message *messages, size_t maxMessages)
{
    size_t count = 0;
    bool inMultiFrame = false;
    const char *p = reply;
    while (*p) {
        const char *end = p;
        while (*end && *end != '\r' && *end != '\n') {
            ++end;
        }
        char line[kMaxLine];
        compact(p, (size_t)(end - p), line, sizeof(line));
        p = *end ? end + 1 : end;
        char *body = line;
        const char *prompt = strchr(body, '>');
        if (prompt != nullptr) {
            body[prompt - body] = '\0';
        }
        if (body[0] == '\0' || strstr(body, "SEARCHING") != nullptr || strncmp(body, "AT", 2) == 0) {
            continue;
        }
        // "0:", "1:" ... continue the current CAN multi-frame message.
        const bool frame = hexValue(body[0]) >= 0 && body[1] == ':';
        if (frame) {
            if (body[0] == '0' || !inMultiFrame) {
                if (count == maxMessages) {
                    break;
                }
                messages[count++].len = 0;
                inMultiFrame = true;
            }
            if (allHex(body + 2)) {
                appendHex(body + 2, messages[count - 1]);
            }
            continue;
        }
        // A short hex line before "0:" is the total length of the multi-frame message.
        if (strlen(body) <= 3 && allHex(body)) {
            continue;
        }
        inMultiFrame = false;
        if (!allHex(body) || strlen(body) % 2 != 0 || count == maxMessages) {
            continue;
        }
        messages[count].len = 0;
        appendHex(body, messages[count]);
        ++count;
    }
    return count;
}

bool parseMode01(const char *reply, uint8_t pid, uint8_t *data, size_t &len)
{
    Message messages[4];
    const size_t n = parseMessages(reply, messages, 4);
    for (size_t i = 0; i < n; ++i) {
        const Message &m = messages[i];
        for (size_t j = 0; j + 1 < m.len; ++j) {
            if (m.bytes[j] == kMode01Reply && m.bytes[j + 1] == pid) {
                len = m.len - j - 2;
                memcpy(data, m.bytes + j + 2, len);
                return true;
            }
        }
    }
    return false;
}

bool parseSupported(const char *reply, uint8_t base, bool (&supported)[256])
{
    uint8_t data[kMaxReplyBytes];
    size_t len = 0;
    if (!parseMode01(reply, base, data, len) || len < 4) {
        return false;
    }
    const uint32_t bits = (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3];
    for (int i = 0; i < 32; ++i) {
        if (bits & (1UL << (31 - i))) {
            supported[(base + 1 + i) & 0xFF] = true;
        }
    }
    return true;
}

bool parseVoltage(const char *reply, float &volts)
{
    for (const char *p = reply; *p; ++p) {
        if (isdigit((unsigned char)*p)) {
            char *end = nullptr;
            const float v = strtof(p, &end);
            if (end != p && end != nullptr && (*end == 'V' || *end == 'v')) {
                volts = v;
                return true;
            }
            p = end != nullptr && end > p ? end - 1 : p;
        }
    }
    return false;
}

uint8_t dtcMode(DtcKind kind)
{
    switch (kind) {
    case DtcKind::Pending: return 0x07;
    case DtcKind::Permanent: return 0x0A;
    case DtcKind::Stored:
    default: return 0x03;
    }
}

void formatDtc(uint8_t a, uint8_t b, char (&code)[kDtcCodeLength])
{
    static const char kLetters[4] = {'P', 'C', 'B', 'U'};
    snprintf(code, sizeof(code), "%c%d%X%X%X", kLetters[a >> 6], (a >> 4) & 0x3, a & 0xF, b >> 4, b & 0xF);
}

size_t parseDtcs(const char *reply, DtcKind kind, bool can, Dtc *out, size_t maxOut)
{
    const uint8_t replyMode = (uint8_t)(dtcMode(kind) + kReplyOffset);
    Message messages[6];
    const size_t n = parseMessages(reply, messages, 6);
    size_t found = 0;
    for (size_t i = 0; i < n; ++i) {
        const Message &m = messages[i];
        if (m.len == 0 || m.bytes[0] != replyMode) {
            continue;
        }
        size_t pos = 1;
        size_t pairs = (m.len - 1) / 2;
        if (can && m.len >= 2) {
            pairs = m.bytes[1];
            pos = 2;
        }
        for (size_t k = 0; k < pairs && pos + 1 < m.len && found < maxOut; ++k) {
            const uint8_t a = m.bytes[pos], b = m.bytes[pos + 1];
            pos += 2;
            if (a == 0 && b == 0) {
                continue;  // padding
            }
            Dtc &d = out[found];
            formatDtc(a, b, d.code);
            d.kind = kind;
            bool duplicate = false;
            for (size_t j = 0; j < found; ++j) {
                duplicate = duplicate || strcmp(out[j].code, d.code) == 0;
            }
            if (!duplicate) {
                ++found;
            }
        }
    }
    return found;
}

const char *dtcSystem(char letter)
{
    switch (letter) {
    case 'P': return "Dong co / hop so";
    case 'C': return "Khung gam";
    case 'B': return "Than xe";
    case 'U': return "Mang giao tiep";
    default: return "He thong";
    }
}

const char *dtcDescription(const char *code)
{
    for (const DtcText &t : kDtcTexts) {
        if (strcmp(t.code, code) == 0) {
            return t.text;
        }
    }
    return nullptr;
}

bool parseVin(const char *reply, char (&vin)[kVinLength + 1])
{
    Message messages[6];
    const size_t n = parseMessages(reply, messages, 6);
    char chars[kMaxReplyBytes];
    size_t count = 0;
    for (size_t i = 0; i < n; ++i) {
        const Message &m = messages[i];
        for (size_t j = 0; j + 2 < m.len; ++j) {
            if (m.bytes[j] == kMode09Reply && m.bytes[j + 1] == kVinPid) {
                for (size_t k = j + 3; k < m.len && count < sizeof(chars); ++k) {
                    const char c = (char)m.bytes[k];
                    if (isalnum((unsigned char)c)) {
                        chars[count++] = (char)toupper((unsigned char)c);
                    }
                }
                break;
            }
        }
    }
    if (count < kVinLength) {
        return false;
    }
    memcpy(vin, chars + count - kVinLength, kVinLength);
    vin[kVinLength] = '\0';
    return true;
}

bool protocolIsCan(const char *atdpnReply)
{
    for (const char *p = atdpnReply; *p; ++p) {
        const char c = (char)toupper((unsigned char)*p);
        if (c == 'A' && p[1] != '\0' && hexValue(p[1]) >= 0) {
            continue;  // "A6": automatic, protocol 6
        }
        const int v = hexValue(c);
        if (v >= 0) {
            return v >= 6 && v <= 0xC;
        }
    }
    return false;
}

float demoValue(uint8_t pid, uint32_t ms)
{
    const float t = ms / 1000.0f;
    const float wave = (sinf(t * 0.35f) + 1) / 2;  // 0..1, slow acceleration and braking
    switch (pid) {
    case 0x0D: return roundf(wave * 82);
    case 0x0C: return roundf(800 + wave * 2600 + 40 * sinf(t * 3));
    case 0x11: return 12 + wave * 45;
    case 0x04: return 20 + wave * 55;
    case 0x05: return fminf(90, 40 + t * 0.8f);
    case 0x0F: return 34 + 2 * sinf(t * 0.05f);
    case 0x2F: return 62;
    case kPidVoltageFromAtrv: return 13.9f + 0.2f * sinf(t);
    case 0x0B: return 30 + wave * 60;
    case 0x10: return 2.5f + wave * 20;
    case 0x03: return 2;
    case 0x06: return 2.3f * sinf(t * 1.7f);
    case 0x07: return 3.1f;
    case 0x0E: return 8 + wave * 20;
    case 0x0A: return 380;
    case 0x14: return 0.45f + 0.4f * sinf(t * 4);
    case 0x46: return 31;
    case 0x5C: return fminf(98, 45 + t * 0.6f);
    case 0x1F: return roundf(t);
    case 0x21: case 0x4D: return 0;
    case 0x31: return 1250;
    case 0x4E: return 3600;
    case 0x33: return 101;
    case 0x43: return 25 + wave * 50;
    case 0x44: return 1.0f;
    case 0x45: case 0x47: case 0x4C: return 10 + wave * 40;
    case 0x49: case 0x4A: case 0x5A: return 15 + wave * 50;
    case 0x5E: return 0.8f + wave * 6;
    case 0x61: case 0x62: return 10 + wave * 60;
    case 0x63: return 250;
    case 0x42: return 14.0f;
    case 0xA6: return 45210.5f + t * wave * 0.02f;
    default: return 0;
    }
}

}  // namespace obd
