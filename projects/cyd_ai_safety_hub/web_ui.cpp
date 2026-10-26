#include "web_ui.h"

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

#include "ai_claude.h"
#include "app_state.h"
#include "report.h"
#include "settings_store.h"
#include "web_page.h"

namespace webui {

namespace {

constexpr uint16_t kPort = 80;
constexpr const char *kUser = "admin";
constexpr size_t kMaxChatChars = 2000;
constexpr uint32_t kMaxSilenceMin = 120;

WebServer server(kPort);

bool auth() {
    String pin;
    {
        SettingsLock l;
        pin = settings::global().pin;
    }
    if (server.authenticate(kUser, pin.c_str())) return true;
    server.requestAuthentication(BASIC_AUTH, "CYD AI Safety Hub", "Sai mat khau (ma PIN tren man hinh)");
    return false;
}

void sendJson(JsonDocument &doc, int code = 200) {
    String out;
    serializeJson(doc, out);
    server.send(code, "application/json", out);
}

void sendError(const char *msg, int code = 400) {
    JsonDocument d;
    d["ok"] = false;
    d["error"] = msg;
    sendJson(d, code);
}

void setNum(JsonObject o, const char *key, float v) {
    if (isnan(v)) {
        o[key] = nullptr;
    } else {
        o[key] = v;
    }
}

bool parseBody(JsonDocument &doc) {
    if (!server.hasArg("plain")) return false;
    return !deserializeJson(doc, server.arg("plain"));
}

void handleIndex() {
    if (!auth()) return;
    server.send_P(200, "text/html; charset=utf-8", kIndexHtml);
}

void handleState() {
    if (!auth()) return;
    Snapshot s;
    static History h;  // static: tránh 720 byte trên stack của loop()
    {
        StateLock l;
        s = AppState::snap;
        h = AppState::hist;
    }
    char reasons[128];
    JsonDocument d;
    JsonObject o = d.to<JsonObject>();
    o["level"] = static_cast<int>(s.level);
    o["reasons"] = s.level == safety::LEVEL_SAFE ? "" : safety::describeReasons(s.reasons, reasons, sizeof(reasons));
    o["silenced"] = s.silenced;
    o["air"] = safety::airNameVi(s.air);
    setNum(o, "co2", s.co2);
    setNum(o, "room_t", s.roomTempC);
    setNum(o, "hum", s.humidity);
    setNum(o, "charger_t", s.chargerTempC);
    setNum(o, "rise", s.riseCPerMin);
    setNum(o, "smoke", s.smokePct);
    if (!s.present.mq2) {
        o["smoke_state"] = "--";
    } else if (isnan(s.smokePct)) {
        o["smoke_state"] = "Đang làm nóng";
    }
    setNum(o, "v", s.voltageV);
    setNum(o, "a", s.currentA);
    o["charging"] = s.charging;
    o["charge_min"] = s.chargeMs / 60000.0f;
    o["wh"] = s.chargeWh;
    o["time"] = report::nowText();
    String sens = String("SCD41 ") + (s.present.scd41 ? "✅" : "❌") + " · MLX90614 " + (s.present.mlx ? "✅" : "❌") + " · " +
                  (s.present.ina228 ? "INA228 " : "INA226 ") + (s.present.ina ? "✅" : "❌") + " · MQ-2 " +
                  (s.present.mq2 ? "✅" : "❌") + " · RAM trống " + String(ESP.getFreeHeap() / 1024) + " KB";
    o["sensors"] = sens;
    JsonArray hist = o["hist"].to<JsonArray>();
    const int start = (h.head - h.count + HISTORY_LEN) % HISTORY_LEN;
    for (int i = 0; i < h.count; ++i) hist.add(h.co2[(start + i) % HISTORY_LEN]);
    sendJson(d);
}

void handleChatPost() {
    if (!auth()) return;
    JsonDocument in;
    if (!parseBody(in)) return sendError("JSON không hợp lệ");
    String text = in["text"] | "";
    text.trim();
    if (text.isEmpty()) return sendError("Câu hỏi trống");
    if (text.length() > kMaxChatChars) text = text.substring(0, kMaxChatChars);
    if (!ai::configured()) return sendError("Chưa nhập Anthropic API key (tab Cài đặt)");
    uint32_t id = 0;
    if (!ai::submit(ai::SRC_WEB, 0, "Web", text, &id)) return sendError("AI đang bận, thử lại sau", 503);
    {
        StateLock l;
        strlcpy(AppState::ai.lastQuestion, "Web: (cau hoi tu trang web)", sizeof(AppState::ai.lastQuestion));
    }
    JsonDocument d;
    d["ok"] = true;
    d["id"] = id;
    sendJson(d);
}

void handleChatGet() {
    if (!auth()) return;
    const uint32_t id = server.arg("id").toInt();
    String reply;
    bool done = false;
    if (!ai::webResult(id, reply, done)) return sendError("Không tìm thấy", 404);
    JsonDocument d;
    d["done"] = done;
    d["reply"] = reply;
    sendJson(d);
}

void handleSettingsGet() {
    if (!auth()) return;
    JsonDocument d;
    safety::Thresholds th;
    {
        StateLock l;
        th = AppState::th;
    }
    {
        SettingsLock l;
        const Settings &s = settings::global();
        d["akey_set"] = s.anthropicKey.length() > 0;
        d["tok_set"] = s.telegramToken.length() > 0;
        d["model"] = s.model;
        d["effort"] = s.effort;
        d["web_search"] = s.webSearch;
        d["plug_on"] = s.plugOnUrl;
        d["plug_off"] = s.plugOffUrl;
        d["auto_cut"] = s.autoCutOnEmergency;
        d["shunt"] = s.shuntOhm;
        int n = 0;
        for (int i = 0; i < MAX_PAIRED_CHATS; ++i) n += s.chats[i] != 0;
        d["chats"] = n;
    }
    d["t_warn"] = th.tempWarnC;
    d["t_danger"] = th.tempDangerC;
    d["t_emerg"] = th.tempEmergencyC;
    d["rise"] = th.riseWarnCPerMin;
    d["s_warn"] = th.smokeWarnPct;
    d["s_danger"] = th.smokeDangerPct;
    d["i_max"] = th.currentMaxA;
    d["v_max"] = th.voltageMaxV;
    d["co2a"] = th.co2AlertPpm;
    d["version"] = APP_NAME " v" APP_VERSION;
    sendJson(d);
}

// Kiểm tra ngưỡng hợp lệ; trả về thông báo lỗi hoặc nullptr
const char *validate(const safety::Thresholds &t) {
    if (!(t.tempWarnC >= 30 && t.tempWarnC < t.tempDangerC && t.tempDangerC < t.tempEmergencyC && t.tempEmergencyC <= 80))
        return "Nhiệt độ phải 30 ≤ theo dõi < nguy cơ < khẩn cấp ≤ 80";
    if (!(t.riseWarnCPerMin >= 0.5f && t.riseWarnCPerMin <= 10)) return "Tốc độ tăng phải 0.5-10 °C/phút";
    if (!(t.smokeWarnPct >= 5 && t.smokeWarnPct < t.smokeDangerPct && t.smokeDangerPct <= 90)) return "Khói phải 5 ≤ theo dõi < nguy cơ ≤ 90";
    if (!(t.currentMaxA == 0 || (t.currentMaxA >= 0.5f && t.currentMaxA <= 60))) return "Dòng tối đa: 0 hoặc 0.5-60 A";
    if (!(t.voltageMaxV == 0 || (t.voltageMaxV >= 5 && t.voltageMaxV <= 120))) return "Áp tối đa: 0 hoặc 5-120 V";
    if (!(t.co2AlertPpm >= 600 && t.co2AlertPpm <= 5000)) return "Báo CO₂: 600-5000 ppm";
    return nullptr;
}

void handleSettingsPost() {
    if (!auth()) return;
    JsonDocument in;
    if (!parseBody(in)) return sendError("JSON không hợp lệ");
    safety::Thresholds th;
    {
        StateLock l;
        th = AppState::th;
    }
    auto num = [&](const char *k, float &dst) {
        if (in[k].is<float>()) dst = in[k].as<float>();
    };
    num("t_warn", th.tempWarnC);
    num("t_danger", th.tempDangerC);
    num("t_emerg", th.tempEmergencyC);
    num("rise", th.riseWarnCPerMin);
    num("s_warn", th.smokeWarnPct);
    num("s_danger", th.smokeDangerPct);
    num("i_max", th.currentMaxA);
    num("v_max", th.voltageMaxV);
    num("co2a", th.co2AlertPpm);
    if (const char *err = validate(th)) return sendError(err);

    float shunt = NAN;
    if (in["shunt"].is<float>()) {
        shunt = in["shunt"].as<float>();
        if (!(shunt >= 0.0005f && shunt <= 1.0f)) return sendError("Điện trở shunt phải 0.0005-1 Ω");
    }
    const String effort = in["effort"] | "";
    if (effort.length() && effort != "low" && effort != "medium" && effort != "high") return sendError("effort không hợp lệ");

    {
        SettingsLock l;
        Settings &s = settings::global();
        if (in["akey"].is<const char *>()) s.anthropicKey = String(in["akey"].as<const char *>());
        if (in["tok"].is<const char *>()) s.telegramToken = String(in["tok"].as<const char *>());
        s.anthropicKey.trim();
        s.telegramToken.trim();
        if (in["model"].is<const char *>()) s.model = String(in["model"].as<const char *>());
        if (effort.length()) s.effort = effort;
        if (in["web_search"].is<bool>()) s.webSearch = in["web_search"].as<bool>();
        if (in["plug_on"].is<const char *>()) s.plugOnUrl = String(in["plug_on"].as<const char *>());
        if (in["plug_off"].is<const char *>()) s.plugOffUrl = String(in["plug_off"].as<const char *>());
        if (in["auto_cut"].is<bool>()) s.autoCutOnEmergency = in["auto_cut"].as<bool>();
        if (!isnan(shunt)) s.shuntOhm = shunt;
        settings::saveSecrets(s);
    }
    const bool aiOk = ai::configured();
    {
        StateLock l;
        AppState::th = th;
        AppState::dev.aiConfigured = aiOk;
    }
    settings::saveThresholds(th);
    JsonDocument d;
    d["ok"] = true;
    sendJson(d);
}

void handleSilence() {
    if (!auth()) return;
    JsonDocument in;
    parseBody(in);
    long minutes = in["minutes"] | 10L;
    if (minutes < 0) minutes = 0;
    if (minutes > static_cast<long>(kMaxSilenceMin)) minutes = kMaxSilenceMin;
    {
        StateLock l;
        AppState::snap.silenced = minutes > 0;
        AppState::snap.silenceUntilMs = minutes > 0 ? millis() + minutes * 60000UL : 0;
    }
    JsonDocument d;
    d["ok"] = true;
    sendJson(d);
}

void handleUnpair() {
    if (!auth()) return;
    {
        SettingsLock l;
        Settings &s = settings::global();
        for (int i = 0; i < MAX_PAIRED_CHATS; ++i) s.chats[i] = 0;
        settings::saveChats(s);
    }
    JsonDocument d;
    d["ok"] = true;
    sendJson(d);
}

void handleReboot() {
    if (!auth()) return;
    JsonDocument d;
    d["ok"] = true;
    sendJson(d);
    delay(300);
    ESP.restart();
}

void handleWifiReset() {
    if (!auth()) return;
    JsonDocument d;
    d["ok"] = true;
    sendJson(d);
    delay(300);
    WiFi.disconnect(true, true);  // xóa Wi-Fi đã lưu -> lần khởi động sau vào chế độ cài đặt
    delay(300);
    ESP.restart();
}

}  // namespace

void begin() {
    server.on("/", HTTP_GET, handleIndex);
    server.on("/api/state", HTTP_GET, handleState);
    server.on("/api/chat", HTTP_POST, handleChatPost);
    server.on("/api/chat", HTTP_GET, handleChatGet);
    server.on("/api/settings", HTTP_GET, handleSettingsGet);
    server.on("/api/settings", HTTP_POST, handleSettingsPost);
    server.on("/api/silence", HTTP_POST, handleSilence);
    server.on("/api/unpair", HTTP_POST, handleUnpair);
    server.on("/api/reboot", HTTP_POST, handleReboot);
    server.on("/api/wifi_reset", HTTP_POST, handleWifiReset);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    server.begin();
}

void handle() { server.handleClient(); }

}  // namespace webui
