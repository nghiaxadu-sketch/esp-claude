// Calls the Claude Messages API (POST /v1/messages) over HTTPS with streaming, so long replies
// (whole apps) never hit the HTTP read timeout.
#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>

enum class ChatRole : uint8_t { User, Assistant };

struct ChatMessage {
    ChatRole role;
    String text;  // original UTF-8, sent back to Claude as context
};

class ClaudeClient {
public:
    // Called while text arrives with the number of characters received so far.
    using ProgressFn = std::function<void(size_t chars)>;

    void configure(const String &apiKey, const String &model);
    const String &model() const { return model_; }
    bool hasApiKey() const { return apiKey_.length() > 0; }

    // Sends the conversation (must start with a user message and alternate roles).
    // On success fills `reply` with Claude's text; otherwise fills `error` with a short ASCII message.
    // `effort` is low | medium | high | xhigh | max, or "" for the API default.
    bool ask(const char *systemPrompt, const std::vector<ChatMessage> &history, uint32_t maxTokens, const char *effort,
             String &reply, String &error, ProgressFn progress = nullptr);

private:
    String buildRequestBody(const char *systemPrompt, const std::vector<ChatMessage> &history, uint32_t maxTokens,
                            const char *effort) const;
    bool useServerFallback() const;

    String apiKey_;
    String model_;
};
