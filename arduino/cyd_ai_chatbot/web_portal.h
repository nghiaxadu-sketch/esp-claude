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
};

void begin(const Hooks &hooks);
// Starts the server once Wi-Fi is up and serves requests; call from the main loop.
void loop();
bool running();

}  // namespace web_portal
