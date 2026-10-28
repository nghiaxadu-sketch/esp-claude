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
    DeviceSettings d = {LCD_BRIGHTNESS_PERCENT, false, SCREEN_OFF_DEFAULT_SEC, true};
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return d;
    }
    d.brightness = prefs.getUChar(kKeyBrightness, d.brightness);
    d.flipped = prefs.getBool(kKeyFlipped, d.flipped);
    d.screenOffSec = prefs.getUShort(kKeyScreenOff, d.screenOffSec);
    d.sound = prefs.getBool(kKeySound, d.sound);
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
