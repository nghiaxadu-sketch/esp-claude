#include "wifi_manager.h"

#include <WiFi.h>
#include <algorithm>

namespace wifi_manager {
namespace {

constexpr size_t kMaxNetworks = 30;
constexpr uint32_t kPollMs = 250;
constexpr int32_t kRssiBars[] = {-85, -75, -65, -55};  // thresholds for 1..4 bars

}  // namespace

void begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
}

std::vector<WifiNetwork> scan()
{
    std::vector<WifiNetwork> list;
    WiFi.mode(WIFI_STA);
    const int16_t count = WiFi.scanNetworks(false, false);
    for (int16_t i = 0; i < count; ++i) {
        const String name = WiFi.SSID(i);
        if (name.length() == 0) {
            continue;
        }
        const int32_t rssi = WiFi.RSSI(i);
        auto same = std::find_if(list.begin(), list.end(), [&](const WifiNetwork &n) { return n.ssid == name; });
        if (same != list.end()) {
            if (rssi > same->rssi) {
                same->rssi = rssi;
                same->channel = WiFi.channel(i);
            }
            continue;
        }
        list.push_back({name, rssi, WiFi.encryptionType(i) == WIFI_AUTH_OPEN, (uint8_t)WiFi.channel(i)});
    }
    WiFi.scanDelete();
    std::sort(list.begin(), list.end(), [](const WifiNetwork &a, const WifiNetwork &b) { return a.rssi > b.rssi; });
    if (list.size() > kMaxNetworks) {
        list.resize(kMaxNetworks);
    }
    Serial.printf("[wifi] scan: %d found, %u listed\n", count, (unsigned)list.size());
    return list;
}

bool connect(const String &ssidName, const String &password, uint32_t timeoutMs)
{
    if (ssidName.length() == 0) {
        return false;
    }
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssidName.c_str(), password.length() > 0 ? password.c_str() : nullptr);
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(kPollMs);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[wifi] connect to \"%s\" failed (status %d)\n", ssidName.c_str(), (int)WiFi.status());
        return false;
    }
    Serial.printf("[wifi] connected to \"%s\", IP %s\n", ssidName.c_str(), WiFi.localIP().toString().c_str());
    return true;
}

bool connected()
{
    return WiFi.status() == WL_CONNECTED;
}

String ssid()
{
    return WiFi.SSID();
}

String ip()
{
    return WiFi.localIP().toString();
}

uint8_t bars(int32_t rssi)
{
    uint8_t n = 0;
    for (int32_t threshold : kRssiBars) {
        if (rssi >= threshold) {
            ++n;
        }
    }
    return n;
}

}  // namespace wifi_manager
