// Settings kept in NVS (Preferences) so Wi-Fi, API key and touch calibration survive reflashing the sketch.
#pragma once

#include <Arduino.h>

#include "touch_xpt2046.h"

struct AppSettings {
    String wifiSsid;
    String wifiPassword;
    String apiKey;
    String model;
};

namespace settings_store {

// Loads NVS values, falling back to config.h for anything not saved.
AppSettings load();

void saveWifi(const String &ssid, const String &password);
void saveApiKey(const String &apiKey);
void saveModel(const String &model);

bool loadTouchCalibration(TouchCalibration &cal);
void saveTouchCalibration(const TouchCalibration &cal);

}  // namespace settings_store
