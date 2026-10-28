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

// Settings > Display / Sound.
struct DeviceSettings {
    uint8_t brightness;      // percent
    bool flipped;            // picture turned 180 degrees
    uint16_t screenOffSec;   // 0 = never
    bool sound;
};

namespace settings_store {

// Loads NVS values, falling back to config.h for anything not saved.
AppSettings load();

void saveWifi(const String &ssid, const String &password);
void saveApiKey(const String &apiKey);
void saveModel(const String &model);

DeviceSettings loadDevice();
void saveDevice(const DeviceSettings &device);

// Small numbers such as game high scores; `key` is at most 15 characters.
int32_t loadInt(const char *key, int32_t fallback);
void saveInt(const char *key, int32_t value);

bool loadTouchCalibration(TouchCalibration &cal);
void saveTouchCalibration(const TouchCalibration &cal);

}  // namespace settings_store
