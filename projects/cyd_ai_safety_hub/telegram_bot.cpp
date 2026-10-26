#include "telegram_bot.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <string>

#include "ai_claude.h"
#include "app_state.h"
#include "config.h"
#include "net_common.h"
#include "report.h"
#include "settings_store.h"
#include "text_util.h"

namespace telegram {

namespace {

constexpr int kAlertQueueLen = 8;
constexpr uint16_t kHttpTimeoutMs = 20000;
constexpr int kLongPollSec = 8;
constexpr uint32_t kTaskStack = 12288;
constexpr uint8_t kMaxPairFails = 5;
constexpr uint32_t kPairLockMs = 600000;
constexpr uint32_t kDefaultSilenceMin = 10;
constexpr uint32_t kMaxSilenceMin = 120;

QueueHandle_t gAlertQueue = nullptr;
volatile bool gPlugOffRequested = false;
int64_t gOffset = 0;
uint8_t gPairFails = 0;
uint32_t gPairLockUntil = 0;

String token() {
    SettingsLock l;
    return settings::global().telegramToken;
}

String apiUrl(const String &tok, const char *method) { return "https://api.telegram.org/bot" + tok + "/" + method; }

bool postJson(NetworkClientSecure &client, const String &url, const String &body, String *response = nullptr) {
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    http.setConnectTimeout(kHttpTimeoutMs);
    http.setReuse(false);  // client có thể dùng chung với máy chủ khác (task AI)
    client.stop();
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");
    const int code = http.POST(body);
    if (response && code > 0) *response = http.getString();
    http.end();
    return code == 200;
}

void setSilence(uint32_t minutes) {
    StateLock l;
    if (minutes == 0) {
        AppState::snap.silenced = false;
        AppState::snap.silenceUntilMs = 0;
    } else {
        AppState::snap.silenced = true;
        AppState::snap.silenceUntilMs = millis() + minutes * 60000UL;
    }
}

String helpText(bool paired) {
    String t = "🤖 " APP_NAME " v" APP_VERSION "\n\n";
    if (!paired) {
        t += "Bạn chưa được ghép đôi. Gửi: /pair <mã PIN 6 số trên màn hình thiết bị>\n";
        return t;
    }
    t += "Nhắn bất kỳ câu hỏi nào để trò chuyện với AI (Claude): hỏi đáp, dịch, viết, tính toán, tra cứu web, "
         "hỏi về không khí trong phòng, tình trạng sạc xe...\n\n";
    t += "Lệnh nhanh:\n";
    t += "/status – xem số liệu\n";
    t += "/im [phút] – tắt còi tạm (mặc định 10 phút)\n";
    t += "/batcoi – bật lại còi\n";
    t += "/nguong – xem ngưỡng cảnh báo\n";
    t += "/ngatsac – ngắt ổ cắm thông minh\n";
    t += "/batsac – bật lại ổ cắm thông minh\n";
    t += "/moi – bắt đầu cuộc trò chuyện mới với AI\n";
    return t;
}

// Xử lý lệnh; trả về true nếu là lệnh (không chuyển cho AI)
bool handleCommand(NetworkClientSecure &client, int64_t chatId, const String &text, bool paired) {
    if (!text.startsWith("/")) return false;
    String cmd = text;
    String arg;
    const int sp = text.indexOf(' ');
    if (sp > 0) {
        cmd = text.substring(0, sp);
        arg = text.substring(sp + 1);
        arg.trim();
    }
    const int at = cmd.indexOf('@');  // "/status@TenBot" trong nhóm
    if (at > 0) cmd = cmd.substring(0, at);
    cmd.toLowerCase();

    if (cmd == "/start" || cmd == "/help") {
        sendMessage(client, chatId, helpText(paired));
        return true;
    }
    if (cmd == "/pair") {
        if (paired) {
            sendMessage(client, chatId, "✅ Chat này đã được ghép đôi rồi.");
            return true;
        }
        if (millis() < gPairLockUntil) {
            sendMessage(client, chatId, "⛔ Nhập sai quá nhiều lần, thử lại sau 10 phút.");
            return true;
        }
        String pin;
        {
            SettingsLock l;
            pin = settings::global().pin;
        }
        if (arg == pin) {
            gPairFails = 0;
            if (settings::addChat(chatId)) {
                sendMessage(client, chatId, "✅ Ghép đôi thành công! Bạn sẽ nhận cảnh báo từ thiết bị.\n\n" + helpText(true));
            } else {
                sendMessage(client, chatId, "⚠️ Đã đủ " + String(MAX_PAIRED_CHATS) + " người dùng. Xóa bớt trong trang web cài đặt.");
            }
        } else {
            if (++gPairFails >= kMaxPairFails) {
                gPairFails = 0;
                gPairLockUntil = millis() + kPairLockMs;
            }
            sendMessage(client, chatId, "❌ Sai mã PIN.");
        }
        return true;
    }
    if (!paired) {
        sendMessage(client, chatId, helpText(false));
        return true;
    }
    if (cmd == "/status") {
        Snapshot s;
        safety::Thresholds th;
        {
            StateLock l;
            s = AppState::snap;
            th = AppState::th;
        }
        sendMessage(client, chatId, report::statusText(s, th));
    } else if (cmd == "/im") {
        long minutes = arg.length() ? arg.toInt() : kDefaultSilenceMin;
        if (minutes <= 0) minutes = kDefaultSilenceMin;
        if (minutes > static_cast<long>(kMaxSilenceMin)) minutes = kMaxSilenceMin;
        setSilence(minutes);
        sendMessage(client, chatId, "🔕 Đã tắt còi " + String(minutes) + " phút. Đèn và tin nhắn cảnh báo vẫn hoạt động.");
    } else if (cmd == "/batcoi") {
        setSilence(0);
        sendMessage(client, chatId, "🔔 Đã bật lại còi.");
    } else if (cmd == "/nguong") {
        safety::Thresholds th;
        {
            StateLock l;
            th = AppState::th;
        }
        sendMessage(client, chatId, report::thresholdsText(th));
    } else if (cmd == "/ngatsac" || cmd == "/batsac") {
        String url;
        {
            SettingsLock l;
            url = cmd == "/ngatsac" ? settings::global().plugOffUrl : settings::global().plugOnUrl;
        }
        if (url.isEmpty()) {
            sendMessage(client, chatId, "Chưa cài ổ cắm thông minh (nhập URL bật/tắt trong trang web cài đặt).");
        } else {
            const bool ok = net::callPlugUrl(url);
            sendMessage(client, chatId, ok ? "✅ Đã gửi lệnh tới ổ cắm." : "❌ Không gọi được ổ cắm thông minh.");
        }
    } else if (cmd == "/moi") {
        ai::resetHistory();
        sendMessage(client, chatId, "🆕 Đã bắt đầu cuộc trò chuyện mới.");
    } else {
        return false;  // lệnh lạ -> để AI trả lời
    }
    return true;
}

void handleUpdate(NetworkClientSecure &client, int64_t chatId, const String &name, const String &text) {
    const bool paired = settings::isChatPaired(chatId);
    if (text.isEmpty()) {
        if (paired) sendMessage(client, chatId, "Hiện mình chỉ đọc được tin nhắn chữ 🙏");
        return;
    }
    if (handleCommand(client, chatId, text, paired)) return;
    if (!paired) {
        sendMessage(client, chatId, helpText(false));
        return;
    }
    if (!ai::configured()) {
        sendMessage(client, chatId, "Chưa nhập Anthropic API key. Vào trang web của thiết bị để cài đặt.");
        return;
    }
    {
        StateLock l;
        String q = name + ": " + text;
        const std::string folded = textutil::foldToAscii(q.c_str());
        strlcpy(AppState::ai.lastQuestion, folded.c_str(), sizeof(AppState::ai.lastQuestion));
    }
    if (!ai::submit(ai::SRC_TELEGRAM, chatId, name, text, nullptr)) {
        sendMessage(client, chatId, "⏳ AI đang bận trả lời câu khác, bạn thử lại sau ít giây nhé.");
    }
}

void pollUpdates(NetworkClientSecure &client, const String &tok) {
    const String url = apiUrl(tok, "getUpdates") + "?timeout=" + String(kLongPollSec) + "&limit=5&offset=" +
                       String(static_cast<long long>(gOffset)) + "&allowed_updates=" + net::urlEncode("[\"message\"]");
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    http.setConnectTimeout(kHttpTimeoutMs);
    http.useHTTP10(true);  // không chunked -> đọc stream trực tiếp
    http.setReuse(false);
    client.stop();
    if (!http.begin(client, url)) return;
    const int code = http.GET();
    if (code != 200) {
        http.end();
        {
            StateLock l;
            AppState::dev.telegramOk = false;
        }
        delay(code == 401 || code == 404 ? 30000 : 3000);  // token sai -> chờ lâu
        return;
    }
    JsonDocument filter;
    JsonObject f = filter["result"][0].to<JsonObject>();
    f["update_id"] = true;
    f["message"]["chat"]["id"] = true;
    f["message"]["text"] = true;
    f["message"]["from"]["first_name"] = true;
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err) return;
    {
        StateLock l;
        AppState::dev.telegramOk = true;
    }
    for (JsonObject upd : doc["result"].as<JsonArray>()) {
        const int64_t id = upd["update_id"].as<int64_t>();
        if (id >= gOffset) gOffset = id + 1;
        JsonObject msg = upd["message"];
        if (msg.isNull()) continue;
        const int64_t chatId = msg["chat"]["id"].as<int64_t>();
        const String name = msg["from"]["first_name"] | "Bạn";
        const String text = msg["text"] | "";
        handleUpdate(client, chatId, name, text);
    }
}

void flushAlerts(NetworkClientSecure &client) {
    char *msg = nullptr;
    while (xQueueReceive(gAlertQueue, &msg, 0) == pdTRUE) {
        int64_t chats[MAX_PAIRED_CHATS];
        {
            SettingsLock l;
            memcpy(chats, settings::global().chats, sizeof(chats));
        }
        for (int i = 0; i < MAX_PAIRED_CHATS; ++i) {
            if (chats[i] != 0) sendMessage(client, chats[i], String(msg));
        }
        free(msg);
    }
}

void task(void *) {
    NetworkClientSecure client;
    net::setupSecureClient(client);
    for (;;) {
        if (gPlugOffRequested && net::online()) {
            gPlugOffRequested = false;
            String url;
            {
                SettingsLock l;
                url = settings::global().plugOffUrl;
            }
            const bool ok = net::callPlugUrl(url);
            char *m = strdup(ok ? "🔌 Đã tự động NGẮT ổ cắm sạc." : "⚠️ Tự động ngắt ổ cắm sạc THẤT BẠI – hãy ngắt bằng tay!");
            if (m && xQueueSend(gAlertQueue, &m, 0) != pdTRUE) free(m);
        }
        const String tok = token();
        if (!net::online() || tok.isEmpty()) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        flushAlerts(client);
        pollUpdates(client, tok);
        flushAlerts(client);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

}  // namespace

void begin() {
    gAlertQueue = xQueueCreate(kAlertQueueLen, sizeof(char *));
    xTaskCreatePinnedToCore(task, "telegram", kTaskStack, nullptr, 2, nullptr, 0);
}

bool broadcast(const String &text) {
    if (!gAlertQueue) return false;
    char *m = strdup(text.c_str());
    if (!m) return false;
    if (xQueueSend(gAlertQueue, &m, 0) != pdTRUE) {
        free(m);
        return false;
    }
    return true;
}

void requestPlugOff() { gPlugOffRequested = true; }

bool sendMessage(NetworkClientSecure &client, int64_t chatId, const String &text) {
    const String tok = token();
    if (tok.isEmpty() || chatId == 0) return false;
    const std::vector<std::string> parts = textutil::splitUtf8(text.c_str(), TELEGRAM_MSG_MAX);
    bool ok = true;
    for (const std::string &part : parts) {
        JsonDocument d;
        d["chat_id"] = chatId;
        d["text"] = part.c_str();
        d["disable_web_page_preview"] = true;
        String body;
        serializeJson(d, body);
        ok = postJson(client, apiUrl(tok, "sendMessage"), body) && ok;
    }
    return ok;
}

void sendTyping(NetworkClientSecure &client, int64_t chatId) {
    const String tok = token();
    if (tok.isEmpty()) return;
    JsonDocument d;
    d["chat_id"] = chatId;
    d["action"] = "typing";
    String body;
    serializeJson(d, body);
    postJson(client, apiUrl(tok, "sendChatAction"), body);
}

}  // namespace telegram
