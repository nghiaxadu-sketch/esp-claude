// Small web page on the board (http://<board IP>/) for things that are slow to type on the touch
// keyboard: the API key, the model, and app descriptions with proper Vietnamese diacritics.
#pragma once

#include <Arduino.h>
#include <functional>

namespace web_portal {

struct Hooks {
    std::function<void(const String &apiKey, const String &model)> saveSettings;  // empty key = keep
    std::function<void(const String &description)> requestCreate;                 // handled by the main loop
    std::function<String(const String &json)> installApp;                         // "" = ok, else error
    std::function<String()> currentModel;
    std::function<bool()> hasApiKey;
    // Wi-Fi chosen on the page (normal mode or the setup hotspot); handled by the main loop.
    std::function<void(const String &ssid, const String &password)> saveWifi;
};

void begin(const Hooks &hooks);
// Starts the server once Wi-Fi is up and serves requests; call from the main loop.
void loop();
bool running();

// "Che do cau hinh Wi-Fi": the board opens its own open hotspot `apName` (http://192.168.4.1, every
// address leads there) so a phone can pick the home network. Stops by itself after the Wi-Fi is saved.
bool startAccessPoint(const String &apName);
void stopAccessPoint();
bool accessPointActive();
String accessPointIp();

}  // namespace web_portal
