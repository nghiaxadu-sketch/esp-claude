#include "obd_link.h"

#include <WiFi.h>

#include "ble_link.h"

namespace {

constexpr uint16_t kAdapterPorts[] = {35000, 23};
constexpr uint32_t kWifiJoinTimeoutMs = 15000;
constexpr uint32_t kTcpConnectTimeoutMs = 3000;
constexpr uint32_t kResetTimeoutMs = 3000;
constexpr uint32_t kCommandTimeoutMs = 1500;
constexpr uint32_t kSearchTimeoutMs = 12000;  // first query: the adapter tries every protocol
constexpr uint32_t kDtcTimeoutMs = 5000;
constexpr uint32_t kProbeRetryMs = 5000;
constexpr uint32_t kVoltageEveryMs = 2000;
constexpr uint32_t kDemoStepMs = 200;
constexpr size_t kMaxReplyChars = 1024;
constexpr size_t kReadChunk = 128;
constexpr int kMaxReconnects = 3;
constexpr char kPrompt = '>';

// Polled every round on the dashboard, in this order.
constexpr uint8_t kDashboardPids[] = {0x0D, 0x0C, 0x05, 0x0F, 0x11, 0x04};
constexpr uint8_t kSupportedBases[] = {0x00, 0x20, 0x40, 0x60, 0x80, 0xA0};

}  // namespace

void ObdSession::reset()
{
    state_ = ObdState::Idle;
    waiting_ = false;
    command_ = "";
    reply_ = "";
    initIndex_ = 0;
    probeStep_ = 0;
    probeBase_ = 0;
    retryAtMs_ = 0;
    reconnects_ = 0;
    vin_ = "";
    protocol_ = "";
    can_ = false;
    dtcStep_ = 0;
    dtcsRead_ = false;
    dtcs_.clear();
    pollIndex_ = 0;
    memset(supported_, 0, sizeof(supported_));
    memset(readings_, 0, sizeof(readings_));
}

bool ObdSession::startBle(const String &address, uint8_t addressType, const String &name)
{
    stop();
    reset();
    name_ = name.length() > 0 ? name : address;
    String error;
    status_ = "Dang ket noi...";
    if (!ble_link::begin(error) || !ble_link::connectSerial(address, addressType, error)) {
        transport_ = ObdTransport::None;
        fail(error);
        return false;
    }
    transport_ = ObdTransport::Ble;
    state_ = ObdState::Initializing;
    stateStartMs_ = millis();
    status_ = "Dang khoi tao ELM327...";
    return true;
}

bool ObdSession::startWifi(const String &ssid, const String &password)
{
    stop();
    reset();
    name_ = ssid;
    wifiSsid_ = ssid;
    wifiPassword_ = password;
    transport_ = ObdTransport::Wifi;
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    WiFi.begin(ssid.c_str(), password.length() > 0 ? password.c_str() : nullptr);
    state_ = ObdState::Connecting;
    stateStartMs_ = millis();
    status_ = "Dang vao Wi-Fi adapter...";
    return true;
}

void ObdSession::startDemo()
{
    stop();
    reset();
    transport_ = ObdTransport::Demo;
    name_ = "Du lieu mau";
    vin_ = obd::kDemoVin;
    protocol_ = "Demo (CAN 11/500)";
    can_ = true;
    size_t count = 0;
    const obd::PidInfo *table = obd::pidTable(count);
    for (size_t i = 0; i < count; ++i) {
        supported_[table[i].pid] = true;
    }
    state_ = ObdState::Running;
    status_ = "Du lieu mau";
}

void ObdSession::stop()
{
    if (transport_ == ObdTransport::Ble) {
        ble_link::disconnect();
    } else if (transport_ == ObdTransport::Wifi) {
        tcp_.stop();
        WiFi.disconnect(false, false);
        WiFi.setAutoReconnect(true);  // the main loop reconnects the home network
    }
    transport_ = ObdTransport::None;
    state_ = ObdState::Idle;
    waiting_ = false;
}

void ObdSession::fail(const String &text)
{
    state_ = ObdState::Error;
    status_ = text;
    waiting_ = false;
    Serial.printf("[obd] %s\n", text.c_str());
}

bool ObdSession::value(uint8_t pid, float &out) const
{
    if (!readings_[pid].valid) {
        return false;
    }
    out = readings_[pid].value;
    return true;
}

bool ObdSession::supported(uint8_t pid) const
{
    return pid == obd::kPidVoltageFromAtrv || supported_[pid];
}

bool ObdSession::transportWrite(const String &text)
{
    if (transport_ == ObdTransport::Ble) {
        return ble_link::writeBytes((const uint8_t *)text.c_str(), text.length());
    }
    if (transport_ == ObdTransport::Wifi) {
        return tcp_.connected() && tcp_.print(text) == text.length();
    }
    return false;
}

size_t ObdSession::transportRead(char *buf, size_t max)
{
    if (transport_ == ObdTransport::Ble) {
        return ble_link::readBytes(buf, max);
    }
    if (transport_ == ObdTransport::Wifi) {
        size_t n = 0;
        while (n < max && tcp_.available() > 0) {
            buf[n++] = (char)tcp_.read();
        }
        return n;
    }
    return 0;
}

bool ObdSession::transportConnected()
{
    if (transport_ == ObdTransport::Ble) {
        return ble_link::serialConnected();
    }
    if (transport_ == ObdTransport::Wifi) {
        return tcp_.connected();
    }
    return transport_ == ObdTransport::Demo;
}

void ObdSession::send(const char *command, uint32_t timeoutMs)
{
    command_ = command;
    reply_ = "";
    waiting_ = true;
    sentMs_ = millis();
    timeoutMs_ = timeoutMs;
    if (!transportWrite(command_ + "\r")) {
        waiting_ = false;
        fail("Mat ket noi adapter");
    }
}

void ObdSession::requestDtcs()
{
    if (state_ != ObdState::Running) {
        return;
    }
    dtcs_.clear();
    dtcsRead_ = false;
    if (transport_ == ObdTransport::Demo) {
        for (size_t i = 0; i < obd::kDemoDtcCount; ++i) {
            obd::Dtc d = {};
            snprintf(d.code, sizeof(d.code), "%s", obd::kDemoDtcs[i]);
            d.kind = i == 1 ? obd::DtcKind::Pending : obd::DtcKind::Stored;
            dtcs_.push_back(d);
        }
        dtcsRead_ = true;
        return;
    }
    dtcStep_ = 1;
}

void ObdSession::requestClearDtcs()
{
    if (state_ != ObdState::Running) {
        return;
    }
    if (transport_ == ObdTransport::Demo) {
        dtcs_.clear();
        dtcsRead_ = true;
        return;
    }
    dtcStep_ = 10;
}

void ObdSession::nextPoll()
{
    static const uint8_t kDashCount = sizeof(kDashboardPids);
    if (!pollAll_) {
        for (uint8_t tries = 0; tries < kDashCount; ++tries) {
            const uint8_t pid = kDashboardPids[pollIndex_++ % kDashCount];
            if (supported_[pid]) {
                char cmd[8];
                snprintf(cmd, sizeof(cmd), "01%02X", pid);
                send(cmd, kCommandTimeoutMs);
                return;
            }
        }
        return;
    }
    size_t count = 0;
    const obd::PidInfo *table = obd::pidTable(count);
    for (size_t tries = 0; tries < count; ++tries) {
        const obd::PidInfo &p = table[pollIndex_++ % count];
        if (p.pid != obd::kPidVoltageFromAtrv && supported_[p.pid]) {
            char cmd[8];
            snprintf(cmd, sizeof(cmd), "01%02X", p.pid);
            send(cmd, kCommandTimeoutMs);
            return;
        }
    }
}

void ObdSession::nextCommand(uint32_t nowMs)
{
    if (state_ == ObdState::Initializing) {
        if (initIndex_ < obd::kInitCommandCount) {
            const char *cmd = obd::kInitCommands[initIndex_++];
            send(cmd, initIndex_ == 1 ? kResetTimeoutMs : kCommandTimeoutMs);
            return;
        }
        state_ = ObdState::Probing;
        status_ = "Dang do ECU...";
        probeStep_ = 0;
        probeBase_ = 0;
    }
    if (state_ == ObdState::Probing) {
        if ((int32_t)(nowMs - retryAtMs_) < 0) {
            return;
        }
        switch (probeStep_) {
        case 0:
            send("0100", kSearchTimeoutMs);  // handleReply moves on to step 1 when the ECU answers
            return;
        case 1:
            probeStep_ = 2;
            send("ATDPN", kCommandTimeoutMs);
            return;
        case 2:
            for (uint8_t base : kSupportedBases) {
                if (base > probeBase_ && supported_[base]) {
                    probeBase_ = base;
                    char cmd[8];
                    snprintf(cmd, sizeof(cmd), "01%02X", base);
                    send(cmd, kCommandTimeoutMs);
                    return;
                }
            }
            probeStep_ = 3;
            if (supported_[0x00]) {
                send("0902", kDtcTimeoutMs);
                return;
            }
            break;
        default:
            break;
        }
        state_ = ObdState::Running;
        status_ = "Da ket noi";
    }
    if (state_ != ObdState::Running) {
        return;
    }
    if (dtcStep_ >= 1 && dtcStep_ <= 3) {
        static const char *const kDtcCommands[3] = {"03", "07", "0A"};
        send(kDtcCommands[dtcStep_ - 1], kDtcTimeoutMs);
        return;
    }
    if (dtcStep_ == 10) {
        send("04", kDtcTimeoutMs);
        return;
    }
    if (nowMs - lastVoltageMs_ >= kVoltageEveryMs) {
        lastVoltageMs_ = nowMs;
        send("ATRV", kCommandTimeoutMs);
        return;
    }
    nextPoll();
}

void ObdSession::handleReply(const String &command, const String &reply)
{
    const char *text = reply.c_str();
    const char *error = obd::replyError(text);
    if (command == "0100") {
        if (error != nullptr || !obd::parseSupported(text, 0x00, supported_)) {
            status_ = String("ECU khong tra loi - bat chia khoa (ON) / no may. ") + (error != nullptr ? error : "");
            retryAtMs_ = millis() + kProbeRetryMs;
            return;
        }
        supported_[0x00] = true;
        probeStep_ = 1;
        return;
    }
    if (command == "ATDPN") {
        can_ = obd::protocolIsCan(text);
        protocol_ = reply;
        protocol_.trim();
        protocol_ = (can_ ? "CAN (" : "OBD (") + protocol_ + ")";
        return;
    }
    if (command == "0902") {
        probeStep_ = 4;
        char vin[obd::kVinLength + 1];
        if (obd::parseVin(text, vin)) {
            vin_ = vin;
        }
        return;
    }
    if (command == "ATRV") {
        float volts;
        if (obd::parseVoltage(text, volts)) {
            readings_[obd::kPidVoltageFromAtrv] = {volts, true};
        }
        return;
    }
    if (command == "03" || command == "07" || command == "0A") {
        const obd::DtcKind kind = command == "03" ? obd::DtcKind::Stored
                                                  : (command == "07" ? obd::DtcKind::Pending : obd::DtcKind::Permanent);
        obd::Dtc found[16];
        const size_t n = error != nullptr ? 0 : obd::parseDtcs(text, kind, can_, found, 16);
        for (size_t i = 0; i < n; ++i) {
            dtcs_.push_back(found[i]);
        }
        dtcStep_ = dtcStep_ < 3 ? dtcStep_ + 1 : 0;
        dtcsRead_ = dtcStep_ == 0;
        return;
    }
    if (command == "04") {
        dtcStep_ = 1;  // read again to show the result
        dtcs_.clear();
        return;
    }
    if (command.startsWith("01") && command.length() == 4) {
        const uint8_t pid = (uint8_t)strtoul(command.c_str() + 2, nullptr, 16);
        if (state_ == ObdState::Probing) {
            obd::parseSupported(text, pid, supported_);
            return;
        }
        uint8_t data[obd::kMaxReplyBytes];
        size_t len = 0;
        float v;
        if (obd::parseMode01(text, pid, data, len) && obd::decodePid(pid, data, len, v)) {
            readings_[pid] = {v, true};
        }
    }
}

void ObdSession::pump(uint32_t nowMs)
{
    char buf[kReadChunk];
    size_t n;
    while ((n = transportRead(buf, sizeof(buf))) > 0) {
        for (size_t i = 0; i < n; ++i) {
            if (buf[i] == kPrompt) {
                waiting_ = false;
                const String cmd = command_;
                const String reply = reply_;
                handleReply(cmd, reply);
                return;
            }
            if (buf[i] != '\0' && reply_.length() < kMaxReplyChars) {
                reply_ += buf[i];
            }
        }
    }
    if (waiting_ && nowMs - sentMs_ > timeoutMs_) {
        waiting_ = false;
        Serial.printf("[obd] timeout on %s\n", command_.c_str());
        if (command_ == "ATZ") {
            return;  // some clones never send the prompt after a reset
        }
        handleReply(command_, reply_);
    }
}

void ObdSession::loop(uint32_t nowMs)
{
    if (transport_ == ObdTransport::None) {
        return;
    }
    if (transport_ == ObdTransport::Demo) {
        if (nowMs - sentMs_ >= kDemoStepMs) {
            sentMs_ = nowMs;
            size_t count = 0;
            const obd::PidInfo *table = obd::pidTable(count);
            for (size_t i = 0; i < count; ++i) {
                readings_[table[i].pid] = {obd::demoValue(table[i].pid, nowMs), true};
            }
        }
        return;
    }
    if (state_ == ObdState::Connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            const IPAddress gateway = WiFi.gatewayIP();
            for (uint16_t port : kAdapterPorts) {
                if (tcp_.connect(gateway, port, kTcpConnectTimeoutMs)) {
                    tcp_.setNoDelay(true);
                    state_ = ObdState::Initializing;
                    status_ = "Dang khoi tao ELM327...";
                    Serial.printf("[obd] TCP %s:%u\n", gateway.toString().c_str(), port);
                    return;
                }
            }
            fail("Khong tim thay adapter tren Wi-Fi nay (" + gateway.toString() + ")");
        } else if (nowMs - stateStartMs_ > kWifiJoinTimeoutMs) {
            fail("Khong vao duoc Wi-Fi cua adapter");
        }
        return;
    }
    if (state_ == ObdState::Error || state_ == ObdState::Idle) {
        return;
    }
    if (!transportConnected()) {
        if (transport_ == ObdTransport::Wifi && reconnects_ < kMaxReconnects && WiFi.status() == WL_CONNECTED) {
            ++reconnects_;
            status_ = "Dang ket noi lai... (" + String(reconnects_) + ")";
            state_ = ObdState::Connecting;
            return;
        }
        fail("Mat ket noi adapter");
        return;
    }
    if (waiting_) {
        pump(nowMs);
        return;
    }
    nextCommand(nowMs);
}
