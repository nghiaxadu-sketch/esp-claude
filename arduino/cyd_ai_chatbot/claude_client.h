// Calls the Claude Messages API (POST /v1/messages) over HTTPS.
#pragma once

#include <Arduino.h>
#include <vector>

enum class ChatRole : uint8_t { User, Assistant };

struct ChatMessage {
    ChatRole role;
    String text;  // original UTF-8, sent back to Claude as context
};

class ClaudeClient {
public:
    void configure(const String &apiKey, const String &model);
    const String &model() const { return model_; }
    bool hasApiKey() const { return apiKey_.length() > 0; }

    // Sends the conversation (must start with a user message and alternate roles).
    // On success fills `reply` with Claude's text; otherwise fills `error` with a short ASCII message.
    bool ask(const std::vector<ChatMessage> &history, String &reply, String &error);

private:
    String buildRequestBody(const std::vector<ChatMessage> &history) const;
    bool useServerFallback() const;

    String apiKey_;
    String model_;
};
