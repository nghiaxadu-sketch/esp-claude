/*
 * CYD AI Safety Hub – gộp 5 dự án ESP32 vào 1 board CYD (ESP32-2432S028R):
 *   1. Trợ lý AI chatbot (Claude) – chat qua Telegram và trang web
 *   2. Máy đo CO2 / nhiệt độ / độ ẩm phòng (SCD41) + biểu đồ 3 giờ
 *   3. Cảnh báo sớm cháy nổ khi sạc xe điện (MLX90614 + MQ-2 + INA226/INA228), 4 mức cảnh báo
 *   4. Báo về điện thoại qua Telegram (cảnh báo, sạc xong, CO2 cao) + điều khiển ổ cắm thông minh
 *   5. Trang web điều khiển trong mạng LAN (số liệu, chat AI, cài đặt)
 *
 * Board: "ESP32 Dev Module", Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
 * Thư viện: LovyanGFX, ArduinoJson 7, WiFiManager (tzapu). Xem README_VI.md.
 *
 * LƯU Ý AN TOÀN: đây là thiết bị cảnh báo hỗ trợ, KHÔNG thay thế cầu chì/aptomat, bộ sạc chính hãng
 * và đầu báo khói đạt chuẩn PCCC.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_random.h>
#include <time.h>

#include "ai_claude.h"
#include "alarm_output.h"
#include "app_state.h"
#include "config.h"
#include "display_ui.h"
#include "report.h"
#include "safety_logic.h"
#include "sensors.h"
#include "settings_store.h"
#include "telegram_bot.h"
#include "web_ui.h"

namespace {

constexpr uint32_t kRescanPeriodMs = 30000;
constexpr uint32_t kBootButtonHoldMs = 4000;
constexpr uint32_t kTouchSilenceMin = 10;
constexpr int kKeyParamLen = 200;
constexpr int kTokenParamLen = 100;

SensorHub sensors;
AlarmOutput alarmOut;
DisplayUi ui;
WiFiManager wm;
WiFiManagerParameter paramKey("akey", "Anthropic API key (sk-ant-...) – để trống nếu không đổi", "", kKeyParamLen);
WiFiManagerParameter paramToken("tgtoken", "Telegram bot token (từ @BotFather) – để trống nếu không đổi", "", kTokenParamLen);

safety::Monitor monitor;
safety::ChargeSession session;
safety::RiseRate rise;
safety::SmokeBaseline smoke;

uint32_t lastSensorMs = 0;
uint32_t lastRescanMs = 0;
uint32_t lastHistoryMs = 0;
uint32_t lastAlertMs = 0;
uint32_t lastCo2AlertMs = 0;
bool co2AlertArmed = true;
bool mlxSeenWhileCharging = false;
bool webStarted = false;
bool timeStarted = false;
bool autoCutDone = false;
uint32_t bootBtnDownMs = 0;
char apName[24];
char apPass[12];

// ---------------- Wi-Fi / cài đặt ----------------

void onPortalSave() {
    const String key = paramKey.getValue();
    const String tok = paramToken.getValue();
    SettingsLock l;
    Settings &s = settings::global();
    if (key.length()) s.anthropicKey = key;
    if (tok.length()) s.telegramToken = tok;
    s.anthropicKey.trim();
    s.telegramToken.trim();
    settings::saveSecrets(s);
}

void setPortalState(bool active) {
    StateLock l;
    AppState::dev.portalActive = active;
    strlcpy(AppState::dev.portalSsid, apName, sizeof(AppState::dev.portalSsid));
    strlcpy(AppState::dev.portalPass, apPass, sizeof(AppState::dev.portalPass));
}

void startPortal() {
    if (webStarted) return;  // cổng 80 đang dùng cho trang web -> cài đặt qua trang web
    setPortalState(true);
    ui.showPortal(apName, apPass);
    wm.startConfigPortal(apName, apPass);
}

void setupWifi() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apName, sizeof(apName), "CYD-AI-%02X%02X", mac[4], mac[5]);
    snprintf(apPass, sizeof(apPass), "%08lu", static_cast<unsigned long>(esp_random() % 100000000UL));

    WiFi.mode(WIFI_STA);
    wm.setTitle("CYD AI Safety Hub");
    wm.addParameter(&paramKey);
    wm.addParameter(&paramToken);
    wm.setSaveParamsCallback(onPortalSave);
    wm.setConfigPortalBlocking(false);  // vẫn đo và báo động trong lúc chờ cài đặt
    wm.setConnectTimeout(20);
    wm.setShowInfoUpdate(false);

    if (!wm.autoConnect(apName, apPass)) {
        setPortalState(true);
        ui.showPortal(apName, apPass);
    }
}

void serviceWifi() {
    if (wm.getConfigPortalActive()) {
        wm.process();
    } else {
        bool active;
        {
            StateLock l;
            active = AppState::dev.portalActive;
        }
        if (active) setPortalState(false);
    }
    const bool ok = WiFi.status() == WL_CONNECTED;
    const bool aiOk = ai::configured();
    {
        StateLock l;
        AppState::dev.wifiOk = ok;
        if (ok) strlcpy(AppState::dev.ip, WiFi.localIP().toString().c_str(), sizeof(AppState::dev.ip));
        AppState::dev.aiConfigured = aiOk;
    }
    if (ok && !timeStarted) {
        timeStarted = true;
        configTzTime(TZ_VIETNAM, NTP_SERVER_1, NTP_SERVER_2);
    }
    if (ok && !webStarted && !wm.getConfigPortalActive()) {
        webStarted = true;
        webui::begin();
        Serial.printf("Web: http://%s  (admin / PIN)\n", WiFi.localIP().toString().c_str());
    }
}

void checkBootButton() {
    if (digitalRead(PIN_BOOT_BTN) == LOW) {
        if (bootBtnDownMs == 0) bootBtnDownMs = millis();
        if (millis() - bootBtnDownMs >= kBootButtonHoldMs && !wm.getConfigPortalActive()) {
            bootBtnDownMs = 0;
            if (webStarted) {
                // Đang có trang web chạy: khởi động lại vào chế độ cài đặt
                WiFi.disconnect(true, true);
                delay(200);
                ESP.restart();
            }
            startPortal();
        }
    } else {
        bootBtnDownMs = 0;
    }
}

// ---------------- Cảm biến & cảnh báo ----------------

void silenceFromScreen() {
    StateLock l;
    AppState::snap.silenced = true;
    AppState::snap.silenceUntilMs = millis() + kTouchSilenceMin * 60000UL;
}

void touchFeedback() { alarmOut.beepShort(); }

void sendLevelAlert(const Snapshot &s, safety::Level prev, uint32_t now) {
    lastAlertMs = now;
    telegram::broadcast(report::alertText(s, prev));
}

void processAlerts(const Snapshot &s, safety::Level prev, bool levelChanged, uint32_t now) {
    if (levelChanged) {
        if (s.level > prev || (s.level == safety::LEVEL_SAFE && prev >= safety::LEVEL_WATCH) ||
            (s.level < prev && s.level >= safety::LEVEL_WATCH)) {
            sendLevelAlert(s, prev, now);
        }
        if (s.level < safety::LEVEL_EMERGENCY) autoCutDone = false;
    } else if (s.level == safety::LEVEL_EMERGENCY && now - lastAlertMs >= ALERT_REPEAT_EMERGENCY_MS) {
        sendLevelAlert(s, prev, now);
    } else if (s.level == safety::LEVEL_DANGER && now - lastAlertMs >= ALERT_REPEAT_DANGER_MS) {
        sendLevelAlert(s, prev, now);
    }

    if (s.level == safety::LEVEL_EMERGENCY && !autoCutDone) {
        bool autoCut;
        {
            SettingsLock l;
            autoCut = settings::global().autoCutOnEmergency && settings::global().plugOffUrl.length();
        }
        if (autoCut) telegram::requestPlugOff();
        autoCutDone = true;
    }

    // CO2 cao: báo tối đa 1 lần/giờ, chỉ báo lại sau khi đã xuống dưới ngưỡng
    float co2Limit;
    {
        StateLock l;
        co2Limit = AppState::th.co2AlertPpm;
    }
    if (!isnan(s.co2)) {
        if (s.co2 >= co2Limit && co2AlertArmed && (lastCo2AlertMs == 0 || now - lastCo2AlertMs >= CO2_ALERT_REPEAT_MS)) {
            co2AlertArmed = false;
            lastCo2AlertMs = now;
            telegram::broadcast("🌫️ CO₂ trong phòng cao: " + String(s.co2, 0) + " ppm (" + safety::airNameVi(s.air) +
                                "). Nên mở cửa cho thoáng khí.");
        } else if (s.co2 < co2Limit - 100) {
            co2AlertArmed = true;
        }
    }
}

void readSensors() {
    const uint32_t now = millis();
    SensorReadings r;
    sensors.poll(r);

    float shunt;
    {
        SettingsLock l;
        shunt = settings::global().shuntOhm;
    }
    sensors.setShunt(shunt);

    const float risePerMin = rise.update(r.chargerTempC, now);
    const float smokePct = smoke.update(r.mq2Mv, now);
    const safety::ChargeSession::Event ev = session.update(r.currentA, r.voltageV, now);
    if (session.charging() && sensors.hasMlx()) mlxSeenWhileCharging = true;
    if (!session.charging()) mlxSeenWhileCharging = false;

    safety::Inputs in;
    in.chargerTempC = r.chargerTempC;
    in.tempRiseCPerMin = risePerMin;
    in.smokePct = smokePct;
    in.currentA = r.currentA;
    in.voltageV = r.voltageV;
    in.sensorFault = session.charging() && mlxSeenWhileCharging && !sensors.hasMlx();

    safety::Thresholds th;
    {
        StateLock l;
        th = AppState::th;
    }
    const safety::Level prev = monitor.level();
    const bool changed = monitor.update(in, th, now);

    Snapshot s;
    {
        StateLock l;
        Snapshot &sn = AppState::snap;
        sn.co2 = r.co2;
        sn.roomTempC = r.roomTempC;
        sn.humidity = r.humidity;
        sn.chargerTempC = r.chargerTempC;
        sn.riseCPerMin = risePerMin;
        sn.smokePct = smokePct;
        sn.smokeMv = r.mq2Mv;
        sn.voltageV = r.voltageV;
        sn.currentA = r.currentA;
        sn.charging = session.charging();
        sn.chargeMs = session.durationMs(now);
        sn.chargeWh = session.energyWh();
        sn.level = monitor.level();
        sn.reasons = monitor.reasons();
        sn.air = safety::airLevel(r.co2);
        if (sn.silenced && static_cast<int32_t>(now - sn.silenceUntilMs) >= 0) sn.silenced = false;
        if (changed && sn.level > prev) sn.silenced = false;  // nguy hiểm tăng thêm -> bật lại còi
        sn.present.scd41 = sensors.hasScd41();
        sn.present.mlx = sensors.hasMlx();
        sn.present.ina = sensors.hasIna();
        sn.present.ina228 = sensors.isIna228();
        sn.present.mq2 = sensors.hasMq2();
        sn.updatedMs = now;
        s = sn;
    }

    processAlerts(s, prev, changed, now);
    if (ev == safety::ChargeSession::EV_STARTED) {
        telegram::broadcast("⚡ Bắt đầu sạc xe (" + String(r.voltageV, 1) + "V, " + String(r.currentA, 2) + "A). Thiết bị đang giám sát.");
    } else if (ev == safety::ChargeSession::EV_FINISHED) {
        telegram::broadcast("✅ Sạc xong! Thời gian " + report::durationText(session.durationMs(now)) + ", khoảng " +
                            String(session.energyWh(), 0) + " Wh. Nên rút sạc để bảo vệ pin.");
    }

    if (now - lastHistoryMs >= HISTORY_PERIOD_MS || lastHistoryMs == 0) {
        lastHistoryMs = now;
        StateLock l;
        AppState::pushHistory(r.co2, r.chargerTempC);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
    AppState::begin();
    {
        SettingsLock l;
        settings::load(settings::global(), AppState::th);
        strlcpy(AppState::dev.pin, settings::global().pin.c_str(), sizeof(AppState::dev.pin));
    }

    alarmOut.begin();
    ui.begin();
    ui.calibrateTouch(ui.touchHeldAtBoot());
    ui.onSilenceRequest(silenceFromScreen);
    ui.onTouchFeedback(touchFeedback);
    ui.showMessage("CYD AI SAFETY HUB", "Dang khoi dong...", "Giu nut BOOT 4 giay = cai lai Wi-Fi");

    float shunt;
    {
        SettingsLock l;
        shunt = settings::global().shuntOhm;
    }
    sensors.begin(shunt);

    setupWifi();
    telegram::begin();
    ai::begin();
    Serial.printf(APP_NAME " v" APP_VERSION " – PIN %s, RAM trống %u\n", AppState::dev.pin, ESP.getFreeHeap());
}

void loop() {
    const uint32_t now = millis();
    serviceWifi();
    checkBootButton();

    if (now - lastSensorMs >= SENSOR_PERIOD_MS) {
        lastSensorMs = now;
        readSensors();
    }
    if (now - lastRescanMs >= kRescanPeriodMs) {
        lastRescanMs = now;
        sensors.rescan();
    }

    safety::Level lv;
    bool silenced;
    bool airBad;
    {
        StateLock l;
        lv = AppState::snap.level;
        silenced = AppState::snap.silenced;
        airBad = AppState::snap.air == safety::AIR_BAD;
    }
    alarmOut.set(lv, silenced, airBad);
    alarmOut.tick();
    ui.tick();
    if (webStarted) webui::handle();
    delay(5);
}
