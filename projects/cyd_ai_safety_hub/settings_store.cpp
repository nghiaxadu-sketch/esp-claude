#include "settings_store.h"

#include <Preferences.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace settings {

namespace {

const char *kNs = "cydhub";
Settings gSettings;
SemaphoreHandle_t gMutex = nullptr;

void ensureMutex() {
    if (!gMutex) gMutex = xSemaphoreCreateRecursiveMutex();
}

String makePin() {
    char buf[7];
    snprintf(buf, sizeof(buf), "%06lu", static_cast<unsigned long>(esp_random() % 1000000UL));
    return String(buf);
}

}  // namespace

Settings &global() { return gSettings; }

void lock() {
    ensureMutex();
    xSemaphoreTakeRecursive(gMutex, portMAX_DELAY);
}

void unlock() { xSemaphoreGiveRecursive(gMutex); }

void load(Settings &s, safety::Thresholds &th) {
    ensureMutex();
    Preferences p;
    p.begin(kNs, false);
    s.anthropicKey = p.getString("akey", "");
    s.telegramToken = p.getString("tgtoken", "");
    s.model = p.getString("model", DEFAULT_MODEL);
    s.effort = p.getString("effort", DEFAULT_EFFORT);
    s.webSearch = p.getBool("websearch", true);
    s.plugOnUrl = p.getString("plugon", "");
    s.plugOffUrl = p.getString("plugoff", "");
    s.autoCutOnEmergency = p.getBool("autocut", true);
    s.shuntOhm = p.getFloat("shunt", DEFAULT_SHUNT_OHM);
    if (!(s.shuntOhm > 0.0001f && s.shuntOhm < 10.0f)) s.shuntOhm = DEFAULT_SHUNT_OHM;
    if (p.getBytesLength("chats") == sizeof(s.chats)) p.getBytes("chats", s.chats, sizeof(s.chats));
    s.pin = p.getString("pin", "");
    if (s.pin.length() != 6) {
        s.pin = makePin();
        p.putString("pin", s.pin);
    }

    const safety::Thresholds d;
    th.tempWarnC = p.getFloat("t_warn", d.tempWarnC);
    th.tempDangerC = p.getFloat("t_danger", d.tempDangerC);
    th.tempEmergencyC = p.getFloat("t_emerg", d.tempEmergencyC);
    th.riseWarnCPerMin = p.getFloat("rise_warn", d.riseWarnCPerMin);
    th.smokeWarnPct = p.getFloat("smk_warn", d.smokeWarnPct);
    th.smokeDangerPct = p.getFloat("smk_danger", d.smokeDangerPct);
    th.currentMaxA = p.getFloat("i_max", d.currentMaxA);
    th.voltageMaxV = p.getFloat("v_max", d.voltageMaxV);
    th.co2AlertPpm = p.getFloat("co2_alert", d.co2AlertPpm);
    p.end();
}

void saveSecrets(const Settings &s) {
    Preferences p;
    p.begin(kNs, false);
    p.putString("akey", s.anthropicKey);
    p.putString("tgtoken", s.telegramToken);
    p.putString("model", s.model);
    p.putString("effort", s.effort);
    p.putBool("websearch", s.webSearch);
    p.putString("plugon", s.plugOnUrl);
    p.putString("plugoff", s.plugOffUrl);
    p.putBool("autocut", s.autoCutOnEmergency);
    p.putFloat("shunt", s.shuntOhm);
    p.end();
}

void saveThresholds(const safety::Thresholds &th) {
    Preferences p;
    p.begin(kNs, false);
    p.putFloat("t_warn", th.tempWarnC);
    p.putFloat("t_danger", th.tempDangerC);
    p.putFloat("t_emerg", th.tempEmergencyC);
    p.putFloat("rise_warn", th.riseWarnCPerMin);
    p.putFloat("smk_warn", th.smokeWarnPct);
    p.putFloat("smk_danger", th.smokeDangerPct);
    p.putFloat("i_max", th.currentMaxA);
    p.putFloat("v_max", th.voltageMaxV);
    p.putFloat("co2_alert", th.co2AlertPpm);
    p.end();
}

void saveChats(const Settings &s) {
    Preferences p;
    p.begin(kNs, false);
    p.putBytes("chats", s.chats, sizeof(s.chats));
    p.end();
}

void saveTouchCalibration(const uint16_t data[8]) {
    Preferences p;
    p.begin(kNs, false);
    p.putBytes("touchcal", data, 8 * sizeof(uint16_t));
    p.end();
}

bool loadTouchCalibration(uint16_t data[8]) {
    Preferences p;
    p.begin(kNs, true);
    bool ok = p.getBytesLength("touchcal") == 8 * sizeof(uint16_t);
    if (ok) p.getBytes("touchcal", data, 8 * sizeof(uint16_t));
    p.end();
    return ok;
}

void factoryReset() {
    Preferences p;
    p.begin(kNs, false);
    p.clear();
    p.end();
}

bool addChat(int64_t chatId) {
    SettingsLock l;
    for (int i = 0; i < MAX_PAIRED_CHATS; ++i) {
        if (gSettings.chats[i] == chatId) return true;
    }
    for (int i = 0; i < MAX_PAIRED_CHATS; ++i) {
        if (gSettings.chats[i] == 0) {
            gSettings.chats[i] = chatId;
            saveChats(gSettings);
            return true;
        }
    }
    return false;
}

bool isChatPaired(int64_t chatId) {
    if (chatId == 0) return false;
    SettingsLock l;
    for (int i = 0; i < MAX_PAIRED_CHATS; ++i) {
        if (gSettings.chats[i] == chatId) return true;
    }
    return false;
}

bool anyChatPaired() {
    SettingsLock l;
    for (int i = 0; i < MAX_PAIRED_CHATS; ++i) {
        if (gSettings.chats[i] != 0) return true;
    }
    return false;
}

}  // namespace settings
