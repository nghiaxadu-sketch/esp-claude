// Settings kept in NVS (Preferences) so Wi-Fi, API key and touch calibration survive reflashing the sketch.
#pragma once

#include <Arduino.h>
#include <vector>

#include "touch_xpt2046.h"

struct AppSettings {
    String wifiSsid;
    String wifiPassword;
    String apiKey;
    String model;
};

// Screen opened right after boot (Settings > Tu khoi dong).
enum class AutoStart : uint8_t { None, Face, Chat, Clock, Calendar, Radio, Music, Obd, Tpms, Count };

// Settings > Display / Sound / Profile / Security / Clock.
struct DeviceSettings {
    uint8_t brightness;      // percent
    bool flipped;            // picture turned 180 degrees
    uint16_t screenOffSec;   // 0 = never
    bool sound;
    uint8_t volume;          // percent: radio, music and beeps
    bool lightTheme;         // "Sang" instead of "Toi"
    uint8_t accent;          // "Giao dien 1..20"
    uint8_t language;        // personas::Language for Claude's replies
    uint8_t persona;         // personas index ("Vai tro AI")
    bool autoGreet;          // greet the user by name after boot
    AutoStart autoStart;
    bool hourlyChime;        // "Bao gio": beep on every full hour (6:00 - 22:00)
    uint8_t clockFace;       // 0 digital, 1 analog
    bool autoTimezone;       // offset from ip-api.com instead of TIMEZONE_OFFSET_SECONDS
    int32_t tzOffsetSec;     // last known offset
    bool lockApps;           // PIN needed to open apps / games / Claude
    bool lockProfile;        // PIN needed to change the AI role, language and name
};

// Settings > Ca nhan (texts).
struct ProfileSettings {
    String userName;
    String locationName;  // "" = MAP_HOME / APP_LOCATION_TEXT
    float latitude;
    float longitude;
    String pin;           // 6 digits, "" = not set
};

// A Wi-Fi network the board has joined before (Settings > Wi-Fi keeps up to kMaxSavedWifi).
struct SavedWifi {
    String ssid;
    String password;
};
constexpr size_t kMaxSavedWifi = 5;

namespace settings_store {

// Loads NVS values, falling back to config.h for anything not saved.
AppSettings load();

void saveWifi(const String &ssid, const String &password);
void saveApiKey(const String &apiKey);
void saveModel(const String &model);

DeviceSettings loadDevice();
void saveDevice(const DeviceSettings &device);

ProfileSettings loadProfile();
void saveProfile(const ProfileSettings &profile);

// Saved networks, most recent first. addWifi() also makes it the network saveWifi() remembers.
std::vector<SavedWifi> loadWifiList();
void addWifi(const String &ssid, const String &password);
void removeWifi(const String &ssid);
// "Xoa Wi-Fi & cau hinh lai": forgets every network.
void clearWifi();
// BSSID (6 bytes) of the access point last joined for ssid, so reconnecting skips the scan.
bool loadBssid(const String &ssid, uint8_t (&bssid)[6]);
void saveBssid(const String &ssid, const uint8_t *bssid);

// Generic small text values (radio stations file path, OBD / TPMS state...). `key` is at most 15 characters.
String loadText(const char *key, const String &fallback = String());
void saveText(const char *key, const String &value);
// Binary blobs (TPMS pairing table...). Returns the number of bytes read.
size_t loadBlob(const char *key, void *data, size_t size);
void saveBlob(const char *key, const void *data, size_t size);

// Small numbers such as game high scores; `key` is at most 15 characters.
int32_t loadInt(const char *key, int32_t fallback);
void saveInt(const char *key, int32_t value);

bool loadTouchCalibration(TouchCalibration &cal);
void saveTouchCalibration(const TouchCalibration &cal);

}  // namespace settings_store
