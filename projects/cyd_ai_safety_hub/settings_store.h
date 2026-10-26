// Lưu cấu hình vào NVS (Preferences): khóa API, token Telegram, ngưỡng, chat đã ghép đôi...
#pragma once
#include <Arduino.h>

#include "config.h"
#include "safety_logic.h"

struct Settings {
    String anthropicKey;
    String telegramToken;
    String model = DEFAULT_MODEL;
    String effort = DEFAULT_EFFORT;
    bool webSearch = true;
    String plugOnUrl;    // ổ cắm thông minh (Tasmota/Shelly...), để trống = không dùng
    String plugOffUrl;
    bool autoCutOnEmergency = true;
    float shuntOhm = DEFAULT_SHUNT_OHM;
    int64_t chats[MAX_PAIRED_CHATS] = {0, 0, 0, 0};
    String pin;          // 6 số, sinh ngẫu nhiên lần đầu
};

namespace settings {

void load(Settings &s, safety::Thresholds &th);
void saveSecrets(const Settings &s);      // key, token, model, effort, webSearch, plug, shunt
void saveThresholds(const safety::Thresholds &th);
void saveChats(const Settings &s);
void saveTouchCalibration(const uint16_t data[8]);
bool loadTouchCalibration(uint16_t data[8]);
void factoryReset();

// Truy cập thread-safe tới bản Settings toàn cục
Settings &global();
void lock();
void unlock();

// Ghép đôi chat Telegram. Trả về true nếu thêm mới thành công (hoặc đã có).
bool addChat(int64_t chatId);
bool isChatPaired(int64_t chatId);
bool anyChatPaired();

}  // namespace settings

class SettingsLock {
public:
    SettingsLock() { settings::lock(); }
    ~SettingsLock() { settings::unlock(); }
    SettingsLock(const SettingsLock &) = delete;
    SettingsLock &operator=(const SettingsLock &) = delete;
};
