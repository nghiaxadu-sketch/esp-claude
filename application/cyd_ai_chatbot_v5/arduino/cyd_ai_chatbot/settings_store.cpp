#include "settings_store.h"

#include <Preferences.h>

#include "config.h"

namespace settings_store {
namespace {

constexpr const char *kNamespace = "cydchat";
constexpr const char *kKeySsid = "ssid";
constexpr const char *kKeyPassword = "pass";
constexpr const char *kKeyApiKey = "apikey";
constexpr const char *kKeyModel = "model";
constexpr const char *kKeyTouchCal = "touchcal";
constexpr const char *kKeyBrightness = "bright";
constexpr const char *kKeyFlipped = "flip";
constexpr const char *kKeyScreenOff = "screenoff";
constexpr const char *kKeySound = "sound";
constexpr const char *kKeyVolume = "volume";
constexpr const char *kKeyLight = "light";
constexpr const char *kKeyAccent = "accent";
constexpr const char *kKeyLanguage = "lang";
constexpr const char *kKeyPersona = "persona";
constexpr const char *kKeyGreet = "greet";
constexpr const char *kKeyAutoStart = "autostart";
constexpr const char *kKeyChime = "chime";
constexpr const char *kKeyClockFace = "clockface";
constexpr const char *kKeyAutoTz = "autotz";
constexpr const char *kKeyTzOffset = "tzoff";
constexpr const char *kKeyLockApps = "lockapps";
constexpr const char *kKeyLockProfile = "lockprof";
constexpr const char *kKeyUserName = "uname";
constexpr const char *kKeyLocation = "locname";
constexpr const char *kKeyLatitude = "loclat";
constexpr const char *kKeyLongitude = "loclon";
constexpr const char *kKeyPin = "pin";
constexpr const char *kKeyWifiList = "wifilist";
constexpr const char *kKeyBssid = "bssid";
constexpr uint8_t kDefaultVolume = 60;
constexpr char kFieldSep = '\t';
constexpr char kRecordSep = '\n';
constexpr size_t kBssidLen = 6;

String readString(Preferences &prefs, const char *key, const char *fallback)
{
    String value = prefs.isKey(key) ? prefs.getString(key, "") : String();
    return value.length() > 0 ? value : String(fallback);
}

void writeString(const char *key, const String &value)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        Serial.println("[settings] NVS open failed");
        return;
    }
    prefs.putString(key, value);
    prefs.end();
}

}  // namespace

AppSettings load()
{
    AppSettings s;
    Preferences prefs;
    // Read-write so the namespace is created on first boot (read-only begin() fails if it does not exist).
    if (!prefs.begin(kNamespace, false)) {
        Serial.println("[settings] NVS open failed, using config.h");
        s.wifiSsid = WIFI_SSID;
        s.wifiPassword = WIFI_PASSWORD;
        s.apiKey = ANTHROPIC_API_KEY;
        s.model = CLAUDE_MODEL;
        return s;
    }
    s.wifiSsid = readString(prefs, kKeySsid, WIFI_SSID);
    s.wifiPassword = readString(prefs, kKeyPassword, WIFI_PASSWORD);
    s.apiKey = readString(prefs, kKeyApiKey, ANTHROPIC_API_KEY);
    s.model = readString(prefs, kKeyModel, CLAUDE_MODEL);
    prefs.end();
    return s;
}

void saveWifi(const String &ssid, const String &password)
{
    writeString(kKeySsid, ssid);
    writeString(kKeyPassword, password);
}

void saveApiKey(const String &apiKey)
{
    writeString(kKeyApiKey, apiKey);
}

void saveModel(const String &model)
{
    writeString(kKeyModel, model);
}

DeviceSettings loadDevice()
{
    DeviceSettings d = {};
    d.brightness = LCD_BRIGHTNESS_PERCENT;
    d.screenOffSec = SCREEN_OFF_DEFAULT_SEC;
    d.sound = true;
    d.volume = kDefaultVolume;
    d.autoStart = AutoStart::None;
    d.tzOffsetSec = TIMEZONE_OFFSET_SECONDS;
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return d;
    }
    d.brightness = prefs.getUChar(kKeyBrightness, d.brightness);
    d.flipped = prefs.getBool(kKeyFlipped, d.flipped);
    d.screenOffSec = prefs.getUShort(kKeyScreenOff, d.screenOffSec);
    d.sound = prefs.getBool(kKeySound, d.sound);
    d.volume = prefs.getUChar(kKeyVolume, d.volume);
    d.lightTheme = prefs.getBool(kKeyLight, false);
    d.accent = prefs.getUChar(kKeyAccent, 0);
    d.language = prefs.getUChar(kKeyLanguage, 0);
    d.persona = prefs.getUChar(kKeyPersona, 0);
    d.autoGreet = prefs.getBool(kKeyGreet, false);
    const uint8_t start = prefs.getUChar(kKeyAutoStart, 0);
    d.autoStart = start < (uint8_t)AutoStart::Count ? (AutoStart)start : AutoStart::None;
    d.hourlyChime = prefs.getBool(kKeyChime, false);
    d.clockFace = prefs.getUChar(kKeyClockFace, 0);
    d.autoTimezone = prefs.getBool(kKeyAutoTz, false);
    d.tzOffsetSec = prefs.getInt(kKeyTzOffset, d.tzOffsetSec);
    d.lockApps = prefs.getBool(kKeyLockApps, false);
    d.lockProfile = prefs.getBool(kKeyLockProfile, false);
    prefs.end();
    return d;
}

void saveDevice(const DeviceSettings &device)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return;
    }
    prefs.putUChar(kKeyBrightness, device.brightness);
    prefs.putBool(kKeyFlipped, device.flipped);
    prefs.putUShort(kKeyScreenOff, device.screenOffSec);
    prefs.putBool(kKeySound, device.sound);
    prefs.putUChar(kKeyVolume, device.volume);
    prefs.putBool(kKeyLight, device.lightTheme);
    prefs.putUChar(kKeyAccent, device.accent);
    prefs.putUChar(kKeyLanguage, device.language);
    prefs.putUChar(kKeyPersona, device.persona);
    prefs.putBool(kKeyGreet, device.autoGreet);
    prefs.putUChar(kKeyAutoStart, (uint8_t)device.autoStart);
    prefs.putBool(kKeyChime, device.hourlyChime);
    prefs.putUChar(kKeyClockFace, device.clockFace);
    prefs.putBool(kKeyAutoTz, device.autoTimezone);
    prefs.putInt(kKeyTzOffset, device.tzOffsetSec);
    prefs.putBool(kKeyLockApps, device.lockApps);
    prefs.putBool(kKeyLockProfile, device.lockProfile);
    prefs.end();
}

ProfileSettings loadProfile()
{
    ProfileSettings p;
    p.latitude = MAP_HOME_LAT;
    p.longitude = MAP_HOME_LON;
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return p;
    }
    p.userName = readString(prefs, kKeyUserName, "");
    p.locationName = readString(prefs, kKeyLocation, "");
    p.latitude = prefs.getFloat(kKeyLatitude, p.latitude);
    p.longitude = prefs.getFloat(kKeyLongitude, p.longitude);
    p.pin = readString(prefs, kKeyPin, "");
    prefs.end();
    return p;
}

void saveProfile(const ProfileSettings &profile)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return;
    }
    prefs.putString(kKeyUserName, profile.userName);
    prefs.putString(kKeyLocation, profile.locationName);
    prefs.putFloat(kKeyLatitude, profile.latitude);
    prefs.putFloat(kKeyLongitude, profile.longitude);
    prefs.putString(kKeyPin, profile.pin);
    prefs.end();
}

std::vector<SavedWifi> loadWifiList()
{
    std::vector<SavedWifi> list;
    const String text = loadText(kKeyWifiList);
    int start = 0;
    while (start < (int)text.length()) {
        int end = text.indexOf(kRecordSep, start);
        if (end < 0) {
            end = text.length();
        }
        const String record = text.substring(start, end);
        const int sep = record.indexOf(kFieldSep);
        if (sep > 0) {
            list.push_back({record.substring(0, sep), record.substring(sep + 1)});
        }
        start = end + 1;
    }
    // Boards upgraded from v5.0 only have the single saved network.
    if (list.empty()) {
        const AppSettings s = load();
        if (s.wifiSsid.length() > 0) {
            list.push_back({s.wifiSsid, s.wifiPassword});
        }
    }
    return list;
}

namespace {

void storeWifiList(const std::vector<SavedWifi> &list)
{
    String text;
    for (const SavedWifi &w : list) {
        text += w.ssid + kFieldSep + w.password + kRecordSep;
    }
    saveText(kKeyWifiList, text);
}

}  // namespace

void addWifi(const String &ssid, const String &password)
{
    std::vector<SavedWifi> list = loadWifiList();
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].ssid == ssid) {
            list.erase(list.begin() + i);
            break;
        }
    }
    list.insert(list.begin(), {ssid, password});
    if (list.size() > kMaxSavedWifi) {
        list.resize(kMaxSavedWifi);
    }
    storeWifiList(list);
    saveWifi(ssid, password);
}

void removeWifi(const String &ssid)
{
    std::vector<SavedWifi> list = loadWifiList();
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].ssid == ssid) {
            list.erase(list.begin() + i);
            break;
        }
    }
    storeWifiList(list);
    saveWifi(list.empty() ? String() : list[0].ssid, list.empty() ? String() : list[0].password);
}

void clearWifi()
{
    storeWifiList({});
    saveWifi(String(), String());
    saveText(kKeyBssid, String());
}

bool loadBssid(const String &ssid, uint8_t (&bssid)[6])
{
    const String text = loadText(kKeyBssid);
    const int sep = text.indexOf(kFieldSep);
    if (sep < 0 || text.substring(0, sep) != ssid || (int)text.length() - sep - 1 != (int)kBssidLen * 2) {
        return false;
    }
    for (size_t i = 0; i < kBssidLen; ++i) {
        bssid[i] = (uint8_t)strtoul(text.substring(sep + 1 + i * 2, sep + 3 + i * 2).c_str(), nullptr, 16);
    }
    return true;
}

void saveBssid(const String &ssid, const uint8_t *bssid)
{
    char hex[kBssidLen * 2 + 1];
    for (size_t i = 0; i < kBssidLen; ++i) {
        snprintf(hex + i * 2, 3, "%02X", bssid[i]);
    }
    saveText(kKeyBssid, ssid + kFieldSep + hex);
}

String loadText(const char *key, const String &fallback)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return fallback;
    }
    const String value = prefs.isKey(key) ? prefs.getString(key, fallback) : fallback;
    prefs.end();
    return value;
}

void saveText(const char *key, const String &value)
{
    writeString(key, value);
}

size_t loadBlob(const char *key, void *data, size_t size)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return 0;
    }
    size_t n = 0;
    if (prefs.isKey(key) && prefs.getBytesLength(key) == size) {
        n = prefs.getBytes(key, data, size);
    }
    prefs.end();
    return n;
}

void saveBlob(const char *key, const void *data, size_t size)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return;
    }
    prefs.putBytes(key, data, size);
    prefs.end();
}

int32_t loadInt(const char *key, int32_t fallback)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return fallback;
    }
    const int32_t value = prefs.getInt(key, fallback);
    prefs.end();
    return value;
}

void saveInt(const char *key, int32_t value)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return;
    }
    prefs.putInt(key, value);
    prefs.end();
}

bool loadTouchCalibration(TouchCalibration &cal)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return false;
    }
    const size_t len = prefs.getBytesLength(kKeyTouchCal);
    bool ok = false;
    if (len == sizeof(TouchCalibration)) {
        ok = prefs.getBytes(kKeyTouchCal, &cal, sizeof(cal)) == sizeof(cal) && cal.valid();
    }
    prefs.end();
    return ok;
}

void saveTouchCalibration(const TouchCalibration &cal)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return;
    }
    prefs.putBytes(kKeyTouchCal, &cal, sizeof(cal));
    prefs.end();
}

}  // namespace settings_store
