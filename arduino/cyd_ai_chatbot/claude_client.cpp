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

// Models that accept `fallbacks: "default"` (re-run a declined request on another model server-side).
constexpr const char *kFallbackModelPrefixes[] = {"claude-opus-5", "claude-fable-5-1", "claude-sonnet-5-5"};

constexpr const char *kSystemPrompt =
    "You are a friendly AI assistant running on a tiny 2.8-inch 320x240 screen (ESP32 \"Cheap Yellow Display\"). "
    "The user types on a small touch keyboard, so questions are short and often Vietnamese without diacritics; "
    "read them as normal Vietnamese. Reply in the user's language. "
    "Keep answers short: at most about 80 words unless the user asks for more detail. "
    "Use plain text only: no Markdown, no tables, no emoji. Short lines and simple '-' lists are fine.";

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

String ClaudeClient::buildRequestBody(const std::vector<ChatMessage> &history) const
{
    JsonDocument doc;
    doc["model"] = model_;
    doc["max_tokens"] = CLAUDE_MAX_TOKENS;
    doc["system"] = kSystemPrompt;
    if (strlen(CLAUDE_EFFORT) > 0) {
        doc["output_config"]["effort"] = CLAUDE_EFFORT;
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

bool ClaudeClient::ask(const std::vector<ChatMessage> &history, String &reply, String &error)
{
    reply = "";
    if (!hasApiKey()) {
        error = "Chua co API key. Go /key sk-ant-... qua Serial Monitor.";
        return false;
    }

    NetworkClientSecure tls;
#if CLAUDE_TLS_INSECURE
    tls.setInsecure();
#else
    tls.setCACertBundle(kCaBundleStart, kCaBundleEnd - kCaBundleStart);
#endif

    HTTPClient http;
    http.useHTTP10(true);  // no chunked encoding, so the JSON can be parsed straight from the stream
    http.setConnectTimeout(kConnectTimeoutMs);
    http.setTimeout(CLAUDE_HTTP_TIMEOUT_MS);
    if (!http.begin(tls, kApiUrl)) {
        error = "Khong mo duoc ket noi HTTPS.";
        return false;
    }
    http.addHeader("content-type", "application/json");
    http.addHeader("x-api-key", apiKey_);
    http.addHeader("anthropic-version", kApiVersion);
    if (useServerFallback()) {
        http.addHeader("anthropic-beta", kFallbackBeta);
    }

    const String body = buildRequestBody(history);
    const uint32_t started = millis();
    const int status = http.POST(body);
    if (status <= 0) {
        error = "Loi mang: " + HTTPClient::errorToString(status);
        http.end();
        return false;
    }

    // Keep only what we read: text blocks (thinking blocks and their signatures are dropped), stop reason, errors.
    JsonDocument filter;
    filter["content"][0]["type"] = true;
    filter["content"][0]["text"] = true;
    filter["stop_reason"] = true;
    filter["error"]["message"] = true;

    JsonDocument doc;
    const DeserializationError jsonError =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    Serial.printf("[claude] HTTP %d in %lu ms\n", status, (unsigned long)(millis() - started));

    if (status != HTTP_CODE_OK) {
        const char *message = doc["error"]["message"] | "";
        error = "API loi " + String(status) + ": " + String(message);
        return false;
    }
    if (jsonError) {
        error = "Doc JSON loi: " + String(jsonError.c_str());
        return false;
    }

    const String stopReason = doc["stop_reason"] | "";
    if (stopReason == "refusal") {
        error = "Claude tu choi tra loi cau nay.";
        return false;
    }
    for (JsonObject block : doc["content"].as<JsonArray>()) {
        if (strcmp(block["type"] | "", "text") == 0) {
            reply += block["text"] | "";
        }
    }
    reply.trim();
    if (stopReason == "max_tokens") {
        reply += reply.length() ? " [...]" : "[Het max_tokens truoc khi tra loi]";
    }
    if (reply.length() == 0) {
        error = "Claude khong tra ve chu nao (stop_reason=" + stopReason + ").";
        return false;
    }
    return true;
}
