#include "ai_claude.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <new>
#include <string>
#include <vector>

#include "app_state.h"
#include "config.h"
#include "net_common.h"
#include "report.h"
#include "settings_store.h"
#include "telegram_bot.h"
#include "text_util.h"

namespace ai {

namespace {

// ---------------- Hằng số ----------------
const char kApiUrl[] = "https://api.anthropic.com/v1/messages";
const char kApiVersion[] = "2026-10-21";
// Beta: định tuyến dự phòng khi bị từ chối + bỏ qua thinking block không còn khớp thay vì lỗi 400
const char kBetaFull[] = "server-side-fallback-2026-07-01,thinking-binding-controls-2026-08-01";
const char kBetaNoFallback[] = "thinking-binding-controls-2026-08-01";
constexpr uint16_t kHttpTimeoutMs = 60000;      // tối đa giữa 2 gói dữ liệu stream
constexpr uint32_t kRoundDeadlineMs = 240000;   // tối đa cho 1 lượt gọi API
constexpr uint32_t kRetryDelayMs = 5000;
constexpr size_t kMaxBlocks = 24;
constexpr int kQueueLen = 3;
constexpr uint32_t kTaskStack = 20480;
constexpr int kWebSlots = 4;
constexpr uint32_t kDisplayRefreshMs = 400;
constexpr size_t kAnswerViewBytes = 650;
constexpr int kWebSearchMaxUses = 3;
constexpr uint32_t kMaxSilenceMin = 120;
constexpr int kHistoryPoints = 12;

const char kSystemPrompt[] =
    "Bạn là \"Cyd\", trợ lý AI chạy trên thiết bị CYD AI Safety Hub đặt trong nhà người dùng ở Hải Phòng, Việt Nam. "
    "Thiết bị đo CO2, nhiệt độ, độ ẩm trong phòng (SCD41) và giám sát an toàn khi sạc xe đạp/xe máy điện "
    "(nhiệt độ bề mặt cục sạc/pin bằng MLX90614, khói/khí gas bằng MQ-2, điện áp và dòng sạc bằng INA226/INA228); "
    "có còi, đèn và gửi cảnh báo về điện thoại qua Telegram.\n\n"
    "Bạn là chatbot đa năng như mọi trợ lý AI: trả lời câu hỏi mọi lĩnh vực, viết, dịch, tóm tắt, giải thích, tính toán, "
    "lên kế hoạch, tư vấn. Khi cần thông tin mới (thời tiết, tin tức, giá cả, sự kiện...) hãy dùng công cụ web_search nếu có.\n\n"
    "Mỗi tin nhắn của người dùng có kèm khối [Dữ liệu thiết bị ...] là số liệu đo thực tế lúc đó. Chỉ dùng khi liên quan "
    "đến câu hỏi; không cần nhắc lại nếu người dùng hỏi chuyện khác.\n\n"
    "An toàn là ưu tiên số 1. Nếu số liệu cho thấy nguy cơ cháy (mức Nguy cơ hoặc KHẨN CẤP), hãy khuyên ngắt điện sạc nếu an toàn, "
    "không dùng nước dập đám cháy pin/điện, rời khỏi nhà và gọi 114 khi có lửa hoặc nhiều khói. Không bao giờ khuyên bỏ qua cảnh báo. "
    "Từ chối đặt ngưỡng an toàn lên mức nguy hiểm và giải thích lý do. Không bật lại ổ cắm sạc khi đang có cảnh báo.\n\n"
    "Khi người dùng nhờ thao tác thiết bị (đổi ngưỡng cảnh báo, tắt/bật còi, ngắt/bật ổ cắm sạc, xem lịch sử số liệu), "
    "hãy dùng công cụ tương ứng rồi báo lại kết quả.\n\n"
    "Trả lời bằng tiếng Việt (trừ khi người dùng viết bằng ngôn ngữ khác), thân thiện, ngắn gọn, đi thẳng vào ý. "
    "Câu trả lời hiển thị trong Telegram dạng chữ thường: KHÔNG dùng bảng Markdown, tiêu đề #, chữ **đậm**; "
    "có thể dùng gạch đầu dòng \"-\", xuống dòng và emoji vừa phải.";

const char kToolsJson[] = R"JSON([
{"name":"set_threshold","description":"Đổi một ngưỡng cảnh báo của thiết bị. temp_*: °C nhiệt độ cục sạc (30-80); rise_warn: °C/phút (0.5-10); smoke_*: % tăng so với nền (5-90); current_max: A (0 = tắt, tối đa 60); voltage_max: V (0 = tắt, tối đa 120); co2_alert: ppm (600-5000). Phải giữ thứ tự theo dõi < nguy cơ < khẩn cấp.","strict":true,
 "input_schema":{"type":"object","properties":{"name":{"type":"string","enum":["temp_warn","temp_danger","temp_emergency","rise_warn","smoke_warn","smoke_danger","current_max","voltage_max","co2_alert"]},"value":{"type":"number"}},"required":["name","value"],"additionalProperties":false}},
{"name":"silence_alarm","description":"Tắt còi báo động tạm thời trong số phút cho trước (1-120). minutes = 0 để bật lại còi ngay. Đèn và tin nhắn cảnh báo vẫn hoạt động.","strict":true,
 "input_schema":{"type":"object","properties":{"minutes":{"type":"integer"}},"required":["minutes"],"additionalProperties":false}},
{"name":"charger_power","description":"Bật (on=true) hoặc ngắt (on=false) ổ cắm thông minh cấp điện cho bộ sạc xe. Chỉ dùng khi người dùng yêu cầu rõ ràng, hoặc ngắt khi có nguy hiểm.","strict":true,
 "input_schema":{"type":"object","properties":{"on":{"type":"boolean"}},"required":["on"],"additionalProperties":false}},
{"name":"get_history","description":"Xem lịch sử 3 giờ gần nhất (mỗi phút 1 mẫu) của CO2 trong phòng hoặc nhiệt độ cục sạc: min, max, trung bình và 12 điểm cách đều.","strict":true,
 "input_schema":{"type":"object","properties":{"sensor":{"type":"string","enum":["co2","charger_temp"]}},"required":["sensor"],"additionalProperties":false}}
])JSON";

// ---------------- Kiểu dữ liệu ----------------
struct Request {
    Source src;
    int64_t chatId;
    uint32_t id;
    String name;
    String text;
};

struct ChatMsg {
    bool user;
    String text;
};

struct WebSlot {
    uint32_t id = 0;
    bool done = false;
    String reply;
};

enum class BlockKind : uint8_t { NONE, TEXT, THINKING, REDACTED, TOOL_USE, SKIP };

struct Block {
    BlockKind kind = BlockKind::NONE;
    String text;       // text / thinking / redacted data / JSON input của tool_use
    String signature;  // chữ ký thinking
    String id;
    String name;
};

struct RoundResult {
    std::vector<Block> blocks;
    String stopReason;
    String error;
    int httpCode = 0;
    bool retryable = false;
};

struct Config {
    String key;
    String model;
    String effort;
    bool webSearch = true;
};

QueueHandle_t gQueue = nullptr;
std::vector<ChatMsg> gHistory;  // chỉ task AI truy cập
volatile bool gResetRequested = false;
WebSlot gWeb[kWebSlots];        // bảo vệ bằng StateLock
uint32_t gNextId = 1;
bool gWebSearchSupported = true;  // tự tắt nếu tài khoản không cho dùng web search

bool modelSupportsFallback(const String &model) {
    return model.startsWith("claude-opus-5") || model.startsWith("claude-fable-5") || model == "claude-sonnet-5-5";
}

// ---------------- Thực thi công cụ ----------------

String toolSetThreshold(JsonObjectConst in, bool &isError) {
    const String name = in["name"] | "";
    const float v = in["value"] | NAN;
    isError = true;
    if (isnan(v)) return "Thiếu giá trị.";
    safety::Thresholds th;
    {
        StateLock l;
        th = AppState::th;
    }
    auto inRange = [](float x, float lo, float hi) { return x >= lo && x <= hi; };
    if (name == "temp_warn") {
        if (!inRange(v, 30, 80) || v >= th.tempDangerC) return "temp_warn phải trong 30-80°C và nhỏ hơn temp_danger (" + String(th.tempDangerC, 0) + ").";
        th.tempWarnC = v;
    } else if (name == "temp_danger") {
        if (!inRange(v, 30, 80) || v <= th.tempWarnC || v >= th.tempEmergencyC)
            return "temp_danger phải trong 30-80°C, lớn hơn temp_warn (" + String(th.tempWarnC, 0) + ") và nhỏ hơn temp_emergency (" + String(th.tempEmergencyC, 0) + ").";
        th.tempDangerC = v;
    } else if (name == "temp_emergency") {
        if (!inRange(v, 30, 80) || v <= th.tempDangerC) return "temp_emergency phải trong 30-80°C và lớn hơn temp_danger (" + String(th.tempDangerC, 0) + ").";
        th.tempEmergencyC = v;
    } else if (name == "rise_warn") {
        if (!inRange(v, 0.5f, 10)) return "rise_warn phải trong 0.5-10 °C/phút.";
        th.riseWarnCPerMin = v;
    } else if (name == "smoke_warn") {
        if (!inRange(v, 5, 90) || v >= th.smokeDangerPct) return "smoke_warn phải trong 5-90% và nhỏ hơn smoke_danger (" + String(th.smokeDangerPct, 0) + ").";
        th.smokeWarnPct = v;
    } else if (name == "smoke_danger") {
        if (!inRange(v, 5, 90) || v <= th.smokeWarnPct) return "smoke_danger phải trong 5-90% và lớn hơn smoke_warn (" + String(th.smokeWarnPct, 0) + ").";
        th.smokeDangerPct = v;
    } else if (name == "current_max") {
        if (!(v == 0 || inRange(v, 0.5f, 60))) return "current_max phải bằng 0 (tắt) hoặc 0.5-60 A.";
        th.currentMaxA = v;
    } else if (name == "voltage_max") {
        if (!(v == 0 || inRange(v, 5, 120))) return "voltage_max phải bằng 0 (tắt) hoặc 5-120 V.";
        th.voltageMaxV = v;
    } else if (name == "co2_alert") {
        if (!inRange(v, 600, 5000)) return "co2_alert phải trong 600-5000 ppm.";
        th.co2AlertPpm = v;
    } else {
        return "Tên ngưỡng không hợp lệ.";
    }
    {
        StateLock l;
        AppState::th = th;
    }
    settings::saveThresholds(th);
    isError = false;
    return "Đã lưu " + name + " = " + String(v, 2) + ".";
}

String toolSilence(JsonObjectConst in, bool &isError) {
    long minutes = in["minutes"] | -1L;
    isError = minutes < 0;
    if (isError) return "Thiếu số phút.";
    if (minutes > static_cast<long>(kMaxSilenceMin)) minutes = kMaxSilenceMin;
    StateLock l;
    if (minutes == 0) {
        AppState::snap.silenced = false;
        AppState::snap.silenceUntilMs = 0;
        return "Đã bật lại còi.";
    }
    AppState::snap.silenced = true;
    AppState::snap.silenceUntilMs = millis() + static_cast<uint32_t>(minutes) * 60000UL;
    return "Đã tắt còi " + String(minutes) + " phút.";
}

String toolChargerPower(JsonObjectConst in, bool &isError) {
    isError = true;
    if (!in["on"].is<bool>()) return "Thiếu tham số on.";
    const bool on = in["on"].as<bool>();
    String url;
    {
        SettingsLock l;
        url = on ? settings::global().plugOnUrl : settings::global().plugOffUrl;
    }
    if (url.isEmpty()) return "Chưa cài ổ cắm thông minh (cần nhập URL bật/tắt trong trang web cài đặt của thiết bị).";
    if (on) {
        StateLock l;
        if (AppState::snap.level >= safety::LEVEL_DANGER) return "Từ chối bật: đang có cảnh báo nguy cơ khi sạc.";
    }
    if (!net::callPlugUrl(url)) return "Không gọi được ổ cắm thông minh.";
    isError = false;
    return on ? "Đã BẬT ổ cắm sạc." : "Đã NGẮT ổ cắm sạc.";
}

String toolHistory(JsonObjectConst in, bool &isError) {
    const String sensor = in["sensor"] | "";
    const bool co2 = sensor == "co2";
    isError = !(co2 || sensor == "charger_temp");
    if (isError) return "sensor phải là co2 hoặc charger_temp.";
    static float vals[HISTORY_LEN];  // static: chỉ task AI gọi, tránh 720 byte trên stack
    int n = 0;
    {
        StateLock l;
        const History &h = AppState::hist;
        const int start = (h.head - h.count + HISTORY_LEN) % HISTORY_LEN;
        for (int i = 0; i < h.count; ++i) {
            const int k = (start + i) % HISTORY_LEN;
            if (co2) {
                vals[n++] = h.co2[k] < 0 ? NAN : static_cast<float>(h.co2[k]);
            } else {
                vals[n++] = h.chargerTempX10[k] == INT16_MIN ? NAN : h.chargerTempX10[k] / 10.0f;
            }
        }
    }
    float mn = NAN, mx = NAN, sum = 0;
    int cnt = 0;
    for (int i = 0; i < n; ++i) {
        if (isnan(vals[i])) continue;
        mn = isnan(mn) ? vals[i] : min(mn, vals[i]);
        mx = isnan(mx) ? vals[i] : max(mx, vals[i]);
        sum += vals[i];
        ++cnt;
    }
    if (cnt == 0) return "Chưa có dữ liệu lịch sử cho cảm biến này.";
    const char *unit = co2 ? "ppm" : "°C";
    const int dec = co2 ? 0 : 1;
    String out = String(cnt) + " mẫu trong " + String(n) + " phút gần nhất. Min " + String(mn, dec) + unit + ", max " +
                 String(mx, dec) + unit + ", TB " + String(sum / cnt, dec) + unit + ". Các điểm (cũ -> mới): ";
    const int step = max(1, n / kHistoryPoints);
    for (int i = 0; i < n; i += step) {
        out += isnan(vals[i]) ? String("--") : String(vals[i], dec);
        out += (i + step < n) ? ", " : "";
    }
    return out;
}

String runTool(const String &name, const String &inputJson, bool &isError) {
    JsonDocument in;
    if (deserializeJson(in, inputJson.length() ? inputJson.c_str() : "{}") || !in.is<JsonObject>()) {
        isError = true;
        return "Tham số không hợp lệ (JSON lỗi).";
    }
    JsonObjectConst obj = in.as<JsonObjectConst>();
    if (name == "set_threshold") return toolSetThreshold(obj, isError);
    if (name == "silence_alarm") return toolSilence(obj, isError);
    if (name == "charger_power") return toolChargerPower(obj, isError);
    if (name == "get_history") return toolHistory(obj, isError);
    isError = true;
    return "Không có công cụ " + name;
}

// ---------------- Gọi API dạng stream (SSE) ----------------

void showPartial(const String &answer, bool force) {
    static uint32_t last = 0;
    if (!force && millis() - last < kDisplayRefreshMs) return;
    last = millis();
    std::string s = answer.c_str();
    if (s.size() > kAnswerViewBytes * 2) s = s.substr(s.size() - kAnswerViewBytes * 2);
    std::string folded = textutil::foldToAscii(s);
    if (folded.size() > kAnswerViewBytes) folded = folded.substr(folded.size() - kAnswerViewBytes);
    StateLock l;
    strlcpy(AppState::ai.lastAnswer, folded.c_str(), sizeof(AppState::ai.lastAnswer));
}

// Đọc 1 ký tự với timeout của stream; -1 nếu hết dữ liệu
int readChar(Stream &s) {
    uint8_t c;
    return s.readBytes(&c, 1) == 1 ? c : -1;
}

void skipLine(Stream &s) {
    int c;
    while ((c = readChar(s)) >= 0 && c != '\n') {
    }
}

Block *blockAt(std::vector<Block> &blocks, int index) {
    if (index < 0 || static_cast<size_t>(index) >= kMaxBlocks) return nullptr;
    if (blocks.size() <= static_cast<size_t>(index)) blocks.resize(index + 1);
    return &blocks[index];
}

void handleEvent(JsonDocument &ev, RoundResult &res, String &answer, bool &stop) {
    const String type = ev["type"] | "";
    if (type == "content_block_start") {
        Block *b = blockAt(res.blocks, ev["index"] | -1);
        if (!b) return;
        JsonObject cb = ev["content_block"];
        const String t = cb["type"] | "";
        if (t == "text") {
            b->kind = BlockKind::TEXT;
            b->text = cb["text"] | "";
            answer += b->text;
        } else if (t == "thinking") {
            b->kind = BlockKind::THINKING;
            b->text = cb["thinking"] | "";
            b->signature = cb["signature"] | "";
        } else if (t == "redacted_thinking") {
            b->kind = BlockKind::REDACTED;
            b->text = cb["data"] | "";
        } else if (t == "tool_use") {
            b->kind = BlockKind::TOOL_USE;
            b->id = cb["id"] | "";
            b->name = cb["name"] | "";
        } else if (t == "fallback") {
            // Đổi sang model dự phòng giữa chừng: bỏ thinking/tool_use trước mốc này, giữ text
            for (Block &prev : res.blocks) {
                if (prev.kind == BlockKind::THINKING || prev.kind == BlockKind::REDACTED || prev.kind == BlockKind::TOOL_USE) {
                    prev.kind = BlockKind::SKIP;
                }
            }
            b->kind = BlockKind::SKIP;
        } else {
            b->kind = BlockKind::SKIP;  // server_tool_use, web_search_tool_result...: không lưu (tiết kiệm RAM)
        }
    } else if (type == "content_block_delta") {
        Block *b = blockAt(res.blocks, ev["index"] | -1);
        if (!b) return;
        JsonObject d = ev["delta"];
        const String dt = d["type"] | "";
        if (dt == "text_delta" && b->kind == BlockKind::TEXT) {
            const char *t = d["text"] | "";
            b->text += t;
            answer += t;
            showPartial(answer, false);
        } else if (dt == "thinking_delta" && b->kind == BlockKind::THINKING) {
            b->text += d["thinking"] | "";
        } else if (dt == "signature_delta" && b->kind == BlockKind::THINKING) {
            b->signature += d["signature"] | "";
        } else if (dt == "input_json_delta" && b->kind == BlockKind::TOOL_USE) {
            b->text += d["partial_json"] | "";
        }
    } else if (type == "message_delta") {
        if (ev["delta"]["stop_reason"].is<const char *>()) res.stopReason = ev["delta"]["stop_reason"].as<const char *>();
    } else if (type == "message_stop") {
        stop = true;
    } else if (type == "error") {
        res.error = ev["error"]["message"] | "Lỗi không rõ từ máy chủ";
        const String et = ev["error"]["type"] | "";
        res.retryable = et == "overloaded_error" || et == "api_error" || et == "rate_limit_error";
        stop = true;
    }
}

bool streamRound(NetworkClientSecure &client, const Config &cfg, const String &body, bool minimal, RoundResult &res,
                 String &answer) {
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    http.setConnectTimeout(20000);
    http.useHTTP10(true);  // tránh chunked encoding để đọc stream trực tiếp
    http.setReuse(false);  // client dùng chung cho 2 máy chủ -> không giữ kết nối cũ
    client.stop();
    if (!http.begin(client, kApiUrl)) {
        res.error = "Không kết nối được api.anthropic.com";
        res.retryable = true;
        return false;
    }
    http.addHeader("content-type", "application/json");
    http.addHeader("x-api-key", cfg.key);
    http.addHeader("anthropic-version", kApiVersion);
    if (!minimal) http.addHeader("anthropic-beta", modelSupportsFallback(cfg.model) ? kBetaFull : kBetaNoFallback);

    const int code = http.POST(body);
    res.httpCode = code;
    if (code != 200) {
        String err;
        if (code > 0) err = http.getString();
        http.end();
        JsonDocument ed;
        if (code > 0 && !deserializeJson(ed, err) && ed["error"]["message"].is<const char *>()) {
            const String msg = ed["error"]["message"].as<const char *>();
            err = msg;
        }
        res.error = "HTTP " + String(code) + ": " + err.substring(0, 300);
        res.retryable = code < 0 || code == 408 || code == 429 || code >= 500;
        return false;
    }

    JsonDocument filter;
    filter["type"] = true;
    filter["index"] = true;
    JsonObject fcb = filter["content_block"].to<JsonObject>();
    for (const char *k : {"type", "id", "name", "text", "thinking", "signature", "data"}) fcb[k] = true;
    JsonObject fd = filter["delta"].to<JsonObject>();
    for (const char *k : {"type", "text", "partial_json", "thinking", "signature", "stop_reason"}) fd[k] = true;
    filter["error"]["type"] = true;
    filter["error"]["message"] = true;

    Stream &st = http.getStream();
    const uint32_t started = millis();
    bool stop = false;
    char key[8];
    while (!stop) {
        if (millis() - started > kRoundDeadlineMs) {
            res.error = "Quá thời gian chờ phản hồi";
            break;
        }
        // Đọc tên trường SSE ("event", "data") tới dấu ':'
        size_t n = 0;
        int c = readChar(st);
        if (c < 0) {
            if (!client.connected()) break;  // máy chủ đóng kết nối
            continue;
        }
        if (c == '\n' || c == '\r') continue;
        while (c >= 0 && c != ':' && c != '\n' && n < sizeof(key) - 1) {
            key[n++] = static_cast<char>(c);
            c = readChar(st);
        }
        key[n] = '\0';
        if (c != ':') {
            if (c != '\n') skipLine(st);
            continue;
        }
        if (strcmp(key, "data") != 0) {
            skipLine(st);
            continue;
        }
        JsonDocument ev;
        const DeserializationError err = deserializeJson(ev, st, DeserializationOption::Filter(filter));
        skipLine(st);
        if (err) {
            res.error = String("Lỗi đọc stream: ") + err.c_str();
            break;
        }
        handleEvent(ev, res, answer, stop);
    }
    http.end();
    if (res.error.length()) return false;
    if (res.stopReason.isEmpty()) {
        res.error = "Kết nối bị ngắt giữa chừng";
        res.retryable = true;
        return false;
    }
    return true;
}

// ---------------- Dựng request ----------------

String buildBody(const Config &cfg, const JsonArray &turn, bool minimal) {
    JsonDocument req;
    req["model"] = cfg.model;
    req["max_tokens"] = AI_MAX_TOKENS;
    req["stream"] = true;
    JsonObject thinking = req["thinking"].to<JsonObject>();
    thinking["type"] = "adaptive";
    if (!minimal) {
        thinking["block_binding"]["prefix_mismatch_behavior"] = "drop_block";
        if (modelSupportsFallback(cfg.model)) req["fallbacks"] = "default";
    }
    req["output_config"]["effort"] = cfg.effort;
    req["system"] = kSystemPrompt;

    JsonDocument tools;
    deserializeJson(tools, kToolsJson);
    JsonArray ta = req["tools"].to<JsonArray>();
    for (JsonVariantConst t : tools.as<JsonArrayConst>()) ta.add(t);
    if (!minimal && cfg.webSearch && gWebSearchSupported) {
        JsonObject ws = ta.add<JsonObject>();
        ws["type"] = "web_search_20260209";
        ws["name"] = "web_search";
        ws["max_uses"] = kWebSearchMaxUses;
        JsonObject loc = ws["user_location"].to<JsonObject>();
        loc["type"] = "approximate";
        loc["city"] = "Hai Phong";
        loc["country"] = "VN";
        loc["timezone"] = "Asia/Ho_Chi_Minh";
    }

    JsonArray msgs = req["messages"].to<JsonArray>();
    for (const ChatMsg &m : gHistory) {
        JsonObject o = msgs.add<JsonObject>();
        o["role"] = m.user ? "user" : "assistant";
        o["content"] = m.text;
    }
    for (JsonVariantConst m : turn) msgs.add(m);

    String body;
    body.reserve(measureJson(req) + 16);
    serializeJson(req, body);
    return body;
}

// Thêm nội dung trả lời của assistant (giữ nguyên thinking + tool_use) vào lượt hiện tại
void appendAssistant(JsonArray turn, const RoundResult &res) {
    JsonObject msg = turn.add<JsonObject>();
    msg["role"] = "assistant";
    JsonArray content = msg["content"].to<JsonArray>();
    for (const Block &b : res.blocks) {
        switch (b.kind) {
            case BlockKind::TEXT:
                if (b.text.length()) {
                    JsonObject o = content.add<JsonObject>();
                    o["type"] = "text";
                    o["text"] = b.text;
                }
                break;
            case BlockKind::THINKING: {
                JsonObject o = content.add<JsonObject>();
                o["type"] = "thinking";
                o["thinking"] = b.text;
                o["signature"] = b.signature;
                break;
            }
            case BlockKind::REDACTED: {
                JsonObject o = content.add<JsonObject>();
                o["type"] = "redacted_thinking";
                o["data"] = b.text;
                break;
            }
            case BlockKind::TOOL_USE: {
                JsonObject o = content.add<JsonObject>();
                o["type"] = "tool_use";
                o["id"] = b.id;
                o["name"] = b.name;
                JsonDocument in;
                if (deserializeJson(in, b.text.length() ? b.text.c_str() : "{}") || !in.is<JsonObject>()) in.to<JsonObject>();
                o["input"] = in.as<JsonObjectConst>();
                break;
            }
            default:
                break;
        }
    }
}

// ---------------- Hội thoại ----------------

void trimHistory() {
    while (gHistory.size() > CHAT_HISTORY_MESSAGES) {
        gHistory.erase(gHistory.begin(), gHistory.begin() + 2);  // bỏ theo cặp hỏi-đáp
    }
}

String friendlyError(const RoundResult &res) {
    if (res.httpCode == 401) return "❌ API key Anthropic không hợp lệ. Kiểm tra lại trong trang web cài đặt.";
    if (res.httpCode == 429) return "⏳ Đang vượt giới hạn gọi API, thử lại sau ít phút.";
    if (res.httpCode == 529 || res.httpCode == 503) return "⏳ Máy chủ AI đang quá tải, thử lại sau ít phút.";
    return "❌ Lỗi khi gọi AI: " + res.error;
}

String converse(NetworkClientSecure &client, const Request &req) {
    Config cfg;
    {
        SettingsLock l;
        cfg.key = settings::global().anthropicKey;
        cfg.model = settings::global().model;
        cfg.effort = settings::global().effort;
        cfg.webSearch = settings::global().webSearch;
    }
    if (cfg.key.isEmpty()) return "Chưa nhập Anthropic API key.";

    Snapshot snap;
    safety::Thresholds th;
    {
        StateLock l;
        snap = AppState::snap;
        th = AppState::th;
    }
    const String userText = req.name + ": " + req.text;

    JsonDocument turnDoc;
    JsonArray turn = turnDoc.to<JsonArray>();
    {
        JsonObject u = turn.add<JsonObject>();
        u["role"] = "user";
        const String content = report::aiContext(snap, th) + "\n\n" + userText;
        u["content"] = content;
    }

    String answer;
    bool minimal = false;
    bool retried = false;
    for (int round = 0; round < AI_MAX_TOOL_ROUNDS; ++round) {
        if (req.src == SRC_TELEGRAM) telegram::sendTyping(client, req.chatId);
        RoundResult res;
        const String body = buildBody(cfg, turn, minimal);
        const size_t answerLenBefore = answer.length();
        if (!streamRound(client, cfg, body, minimal, res, answer)) {
            answer = answer.substring(0, answerLenBefore);  // bỏ phần trả lời dở dang
            if (res.httpCode == 400 && !minimal) {
                // Tự sửa: tài khoản/model không hỗ trợ tính năng nâng cao -> thử lại bản tối giản
                if (res.error.indexOf("web_search") >= 0) gWebSearchSupported = false;
                minimal = true;
                --round;
                continue;
            }
            if (res.retryable && !retried) {
                retried = true;
                vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
                --round;
                continue;
            }
            return friendlyError(res);
        }

        if (res.stopReason == "refusal") return "🙏 Xin lỗi, mình không thể hỗ trợ yêu cầu này.";
        if (res.stopReason != "tool_use") {
            if (res.stopReason == "max_tokens") answer += "\n(…câu trả lời bị cắt do quá dài)";
            if (res.stopReason == "pause_turn" && answer.isEmpty()) answer = "Mình chưa tìm xong, bạn hỏi lại giúp mình nhé.";
            break;
        }

        // Thực thi công cụ rồi gửi kết quả lại cho Claude
        appendAssistant(turn, res);
        JsonObject tr = turn.add<JsonObject>();
        tr["role"] = "user";
        JsonArray results = tr["content"].to<JsonArray>();
        for (const Block &b : res.blocks) {
            if (b.kind != BlockKind::TOOL_USE) continue;
            bool isError = false;
            const String out = runTool(b.name, b.text, isError);
            JsonObject r = results.add<JsonObject>();
            r["type"] = "tool_result";
            r["tool_use_id"] = b.id;
            r["content"] = out;
            if (isError) r["is_error"] = true;
        }
        if (results.size() == 0) break;  // không có công cụ nào hợp lệ
        if (answer.length() && !answer.endsWith("\n")) answer += "\n";
    }
    answer.trim();
    if (answer.isEmpty()) answer = "(Không có nội dung trả lời)";

    gHistory.push_back({true, String(textutil::truncateUtf8(userText.c_str(), CHAT_HISTORY_MAX_BYTES).c_str())});
    gHistory.push_back({false, String(textutil::truncateUtf8(answer.c_str(), CHAT_HISTORY_MAX_BYTES).c_str())});
    trimHistory();
    return answer;
}

void storeWebResult(uint32_t id, const String &reply) {
    StateLock l;
    for (WebSlot &s : gWeb) {
        if (s.id == id) {
            s.reply = reply;
            s.done = true;
            return;
        }
    }
}

void task(void *) {
    NetworkClientSecure client;
    net::setupSecureClient(client);
    for (;;) {
        Request *req = nullptr;
        if (xQueueReceive(gQueue, &req, portMAX_DELAY) != pdTRUE || !req) continue;
        if (gResetRequested) {
            gResetRequested = false;
            gHistory.clear();
        }
        {
            StateLock l;
            AppState::ai.busy = true;
            strlcpy(AppState::ai.lastAnswer, "Dang suy nghi...", sizeof(AppState::ai.lastAnswer));
        }
        String reply = net::online() ? converse(client, *req) : String("📵 Thiết bị đang mất Wi-Fi.");
        showPartial(reply, true);
        {
            StateLock l;
            AppState::ai.busy = false;
        }
        if (req->src == SRC_TELEGRAM) {
            telegram::sendMessage(client, req->chatId, reply);
        } else {
            storeWebResult(req->id, reply);
        }
        delete req;
    }
}

}  // namespace

void begin() {
    gQueue = xQueueCreate(kQueueLen, sizeof(Request *));
    xTaskCreatePinnedToCore(task, "ai", kTaskStack, nullptr, 1, nullptr, 0);
}

bool configured() {
    SettingsLock l;
    return settings::global().anthropicKey.length() > 0;
}

bool submit(Source src, int64_t chatId, const String &userName, const String &text, uint32_t *outId) {
    if (!gQueue) return false;
    Request *r = new (std::nothrow) Request{src, chatId, 0, userName, text};
    if (!r) return false;
    if (src == SRC_WEB) {
        StateLock l;
        r->id = gNextId++;
        WebSlot &slot = gWeb[r->id % kWebSlots];
        slot.id = r->id;
        slot.done = false;
        slot.reply = "";
        if (outId) *outId = r->id;
    }
    if (xQueueSend(gQueue, &r, 0) != pdTRUE) {
        delete r;
        return false;
    }
    return true;
}

bool webResult(uint32_t id, String &reply, bool &done) {
    StateLock l;
    for (const WebSlot &s : gWeb) {
        if (s.id == id && id != 0) {
            reply = s.reply;
            done = s.done;
            return true;
        }
    }
    return false;
}

void resetHistory() { gResetRequested = true; }

}  // namespace ai
