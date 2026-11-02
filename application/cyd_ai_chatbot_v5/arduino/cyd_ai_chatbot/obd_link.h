// One OBD-II session with an ELM327 adapter over Bluetooth LE or over the adapter's own Wi-Fi network
// (TCP 35000 on its gateway address), or with sample data. Commands are sent one at a time without blocking
// the main loop: initialisation, ECU probing (supported PIDs, protocol, VIN), then continuous polling of the
// sensors the screen shows, with fault code read / clear on request.
#pragma once

#include <Arduino.h>
#include <NetworkClient.h>
#include <vector>

#include "obd_codec.h"

enum class ObdTransport : uint8_t { None, Ble, Wifi, Demo };
enum class ObdState : uint8_t { Idle, Connecting, Initializing, Probing, Running, Error };

class ObdSession {
public:
    // Blocking up to a few seconds (BLE connect); false with error() set when it fails.
    bool startBle(const String &address, uint8_t addressType, const String &name);
    // Joins the adapter's Wi-Fi (the home network is left until stop()).
    bool startWifi(const String &ssid, const String &password);
    void startDemo();
    void stop();

    void loop(uint32_t nowMs);

    ObdTransport transport() const { return transport_; }
    ObdState state() const { return state_; }
    // Short ASCII progress / error text for the screen.
    const String &statusText() const { return status_; }
    const String &deviceName() const { return name_; }
    const String &vin() const { return vin_; }
    const String &protocolText() const { return protocol_; }
    bool isCan() const { return can_; }

    // Latest value; false when the sensor was never read or is not supported.
    bool value(uint8_t pid, float &out) const;
    bool supported(uint8_t pid) const;
    // Dashboard only (fast) or every supported sensor ("Tat ca cam bien").
    void setPollAll(bool all) { pollAll_ = all; }

    void requestDtcs();
    // Two-step clear: the screen asks for confirmation first.
    void requestClearDtcs();
    bool dtcsBusy() const { return dtcStep_ != 0; }
    bool dtcsRead() const { return dtcsRead_; }
    const std::vector<obd::Dtc> &dtcs() const { return dtcs_; }

private:
    struct Reading {
        float value;
        bool valid;
    };

    void reset();
    void send(const char *command, uint32_t timeoutMs);
    void pump(uint32_t nowMs);
    void handleReply(const String &command, const String &reply);
    void nextCommand(uint32_t nowMs);
    void nextPoll();
    void fail(const String &text);
    bool transportWrite(const String &text);
    size_t transportRead(char *buf, size_t max);
    bool transportConnected();

    ObdTransport transport_ = ObdTransport::None;
    ObdState state_ = ObdState::Idle;
    String status_;
    String name_;
    String vin_;
    String protocol_;
    bool can_ = false;
    NetworkClient tcp_;
    String wifiSsid_;
    String wifiPassword_;
    uint32_t stateStartMs_ = 0;

    // Command in flight.
    String command_;
    String reply_;
    bool waiting_ = false;
    uint32_t sentMs_ = 0;
    uint32_t timeoutMs_ = 0;
    size_t initIndex_ = 0;
    uint8_t probeStep_ = 0;  // 0 = 01 00, 1 = ATDPN, 2 = other supported-PID bitmaps, 3 = VIN
    uint8_t probeBase_ = 0;
    uint32_t lastVoltageMs_ = 0;
    uint32_t retryAtMs_ = 0;
    int reconnects_ = 0;

    bool supported_[256] = {};
    Reading readings_[256] = {};
    size_t pollIndex_ = 0;
    bool pollAll_ = false;

    uint8_t dtcStep_ = 0;  // 0 idle, 1..3 reading 03/07/0A, 10 clearing
    bool dtcsRead_ = false;
    std::vector<obd::Dtc> dtcs_;
};
