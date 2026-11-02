#include "claude_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include "config.h"

// Root CA bundle that ESP-IDF embeds in the arduino-esp32 core (Mozilla list); used to verify api.anthropic.com.
extern const uint8_t kCaBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t kCaBundleEnd[] asm("_binary_x509_crt_bundle_end");

namespace {

constexpr const char *kApiUrl = "https://api.anthropic.com/v1/messages";
constexpr const char *kApiVersion = "2026-10-21";
constexpr const char *kFallbackBeta = "server-side-fallback-2026-07-01";
constexpr int32_t kConnectTimeoutMs = 15000;
constexpr uint32_t kStreamIdleTimeoutMs = 60000;  // the API sends "ping" events while thinking
constexpr size_t kMaxSseLine = 4096;               // longer lines (thinking signatures) are skipped
constexpr size_t kReadChunk = 128;
constexpr uint32_t kReadPollMs = 2;
constexpr const char *kSseDataPrefix = "data:";

// Models that accept `fallbacks: "default"` (re-run a declined request on another model server-side).
constexpr const char *kFallbackModelPrefixes[] = {"claude-opus-5", "claude-fable-5-1", "claude-sonnet-5-5"};

}  // namespace

void ClaudeClient::configure(const String &apiKey, const String &model)
{
    apiKey_ = apiKey;
    apiKey_.trim();
    model_ = model;
    model_.trim();
}

bool ClaudeClient::useServerFallback() const
{
    for (const char *prefix : kFallbackModelPrefixes) {
        if (model_.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

String ClaudeClient::buildRequestBody(const char *systemPrompt, const std::vector<ChatMessage> &history,
                                      uint32_t maxTokens, const char *effort) const
{
    JsonDocument doc;
    doc["model"] = model_;
    doc["max_tokens"] = maxTokens;
    doc["stream"] = true;
    doc["system"] = systemPrompt;
    if (effort != nullptr && *effort != '\0') {
        doc["output_config"]["effort"] = effort;
    }
    if (useServerFallback()) {
        doc["fallbacks"] = "default";
    }
    JsonArray messages = doc["messages"].to<JsonArray>();
    for (const ChatMessage &m : history) {
        JsonObject msg = messages.add<JsonObject>();
        msg["role"] = m.role == ChatRole::User ? "user" : "assistant";
        msg["content"] = m.text;
    }
    String body;
    serializeJson(doc, body);
    return body;
}

namespace {

// Accumulates the text of a streamed reply from server-sent events.
struct StreamState {
    String reply;
    String stopReason;
    String error;
    bool done = false;
};

void handleSseData(const String &json, StreamState &st)
{
    JsonDocument filter;
    filter["type"] = true;
    filter["delta"]["type"] = true;
    filter["delta"]["text"] = true;
    filter["delta"]["stop_reason"] = true;
    filter["error"]["message"] = true;

    JsonDocument ev;
    if (deserializeJson(ev, json, DeserializationOption::Filter(filter))) {
        return;
    }
    const char *type = ev["type"] | "";
    if (strcmp(type, "content_block_delta") == 0) {
        if (strcmp(ev["delta"]["type"] | "", "text_delta") == 0) {
            st.reply += ev["delta"]["text"] | "";
        }
    } else if (strcmp(type, "message_delta") == 0) {
        const char *stop = ev["delta"]["stop_reason"] | "";
        if (*stop != '\0') {
            st.stopReason = stop;
        }
    } else if (strcmp(type, "message_stop") == 0) {
        st.done = true;
    } else if (strcmp(type, "error") == 0) {
        st.error = String("API loi: ") + (ev["error"]["message"] | "unknown");
        st.done = true;
    }
}

// Reads "data: {...}" lines until message_stop, an error, the connection closing or a stall.
void readEventStream(NetworkClient &stream, StreamState &st, const ClaudeClient::ProgressFn &progress)
{
    String line;
    line.reserve(256);
    bool overflow = false;
    size_t reported = 0;
    uint8_t buf[kReadChunk];
    uint32_t lastData = millis();
    while (!st.done) {
        const int avail = stream.available();
        if (avail <= 0) {
            if (!stream.connected()) {
                break;
            }
            if (millis() - lastData > kStreamIdleTimeoutMs) {
                st.error = "Het thoi gian cho Claude tra loi.";
                return;
            }
            delay(kReadPollMs);
            continue;
        }
        const int n = stream.read(buf, min<int>(avail, (int)sizeof(buf)));
        lastData = millis();
        for (int i = 0; i < n && !st.done; ++i) {
            const char c = (char)buf[i];
            if (c == '\r') {
                continue;
            }
            if (c != '\n') {
                if (line.length() < kMaxSseLine) {
                    line += c;
                } else {
                    overflow = true;
                }
                continue;
            }
            if (!overflow && line.startsWith(kSseDataPrefix)) {
                String data = line.substring(strlen(kSseDataPrefix));
                data.trim();
                handleSseData(data, st);
            }
            line = "";
            overflow = false;
        }
        if (progress && st.reply.length() != reported) {
            reported = st.reply.length();
            progress(reported);
        }
    }
}

}  // namespace

bool ClaudeClient::ask(const char *systemPrompt, const std::vector<ChatMessage> &history, uint32_t maxTokens,
                       const char *effort, String &reply, String &error, ProgressFn progress)
{
    reply = "";
    if (!hasApiKey()) {
        error = "Chua co API key. Go /key sk-ant-... qua Serial hoac vao trang web cua may.";
        return false;
    }

    NetworkClientSecure tls;
#if CLAUDE_TLS_INSECURE
    tls.setInsecure();
#else
    tls.setCACertBundle(kCaBundleStart, kCaBundleEnd - kCaBundleStart);
#endif

    HTTPClient http;
    http.useHTTP10(true);  // no chunked encoding: the event stream is read straight from the socket
    http.setConnectTimeout(kConnectTimeoutMs);
    http.setTimeout(CLAUDE_HTTP_TIMEOUT_MS);
    if (!http.begin(tls, kApiUrl)) {
        error = "Khong mo duoc ket noi HTTPS.";
        return false;
    }
    http.addHeader("content-type", "application/json");
    http.addHeader("accept", "text/event-stream");
    http.addHeader("x-api-key", apiKey_);
    http.addHeader("anthropic-version", kApiVersion);
    if (useServerFallback()) {
        http.addHeader("anthropic-beta", kFallbackBeta);
    }

    const uint32_t started = millis();
    const int status = http.POST(buildRequestBody(systemPrompt, history, maxTokens, effort));
    if (status <= 0) {
        error = "Loi mang: " + HTTPClient::errorToString(status);
        http.end();
        return false;
    }
    if (status != HTTP_CODE_OK) {
        JsonDocument filter;
        filter["error"]["message"] = true;
        JsonDocument doc;
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        http.end();
        const char *message = doc["error"]["message"] | "";
        error = "API loi " + String(status) + ": " + String(message);
        return false;
    }

    StreamState st;
    st.reply.reserve(1024);
    readEventStream(http.getStream(), st, progress);
    http.end();
    Serial.printf("[claude] HTTP %d, %u chars in %lu ms, stop=%s\n", status, (unsigned)st.reply.length(),
                  (unsigned long)(millis() - started), st.stopReason.c_str());

    if (st.error.length() > 0) {
        error = st.error;
        return false;
    }
    if (st.stopReason == "refusal") {
        error = "Claude tu choi tra loi cau nay.";
        return false;
    }
    reply = st.reply;
    reply.trim();
    if (st.stopReason == "max_tokens") {
        reply += reply.length() ? " [...]" : "[Het max_tokens truoc khi tra loi]";
    }
    if (reply.length() == 0) {
        error = "Claude khong tra ve chu nao (stop_reason=" + st.stopReason + ").";
        return false;
    }
    return true;
}
