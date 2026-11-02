// Wi-Fi station helpers: scan nearby networks and connect.
#pragma once

#include <Arduino.h>
#include <vector>

struct WifiNetwork {
    String ssid;  // UTF-8 as broadcast
    int32_t rssi;
    bool open;    // no password needed
    uint8_t channel;
};

namespace wifi_manager {

void begin();
// Blocking scan (2-5 s). Hidden networks are skipped, duplicates keep the strongest, sorted by signal.
std::vector<WifiNetwork> scan();
// Blocking connect with timeout. Returns true when an IP address was obtained.
bool connect(const String &ssid, const String &password, uint32_t timeoutMs);
bool connected();
String ssid();
String ip();
// 0..4 bars from RSSI.
uint8_t bars(int32_t rssi);

}  // namespace wifi_manager
