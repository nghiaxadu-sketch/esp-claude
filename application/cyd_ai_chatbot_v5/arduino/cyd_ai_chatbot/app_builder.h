// Asks Claude to write or change an app, checks the result on the device and lets Claude fix
// rejected apps (up to APP_REPAIR_ROUNDS times) before anything is saved.
#pragma once

#include <Arduino.h>
#include <functional>

#include "claude_client.h"

class AppBuilder {
public:
    // Progress for the screen: a short ASCII status line.
    using StatusFn = std::function<void(const String &status)>;

    explicit AppBuilder(ClaudeClient &claude) : claude_(claude) {}

    // `description` is what the user typed (UTF-8). On success `json` holds a checked app.
    bool create(const String &description, String &json, String &error, StatusFn status);
    // Changes an existing app; `lastError` is its most recent runtime error (may be empty).
    bool edit(const String &currentJson, const String &request, const String &lastError, String &json,
              String &error, StatusFn status);

private:
    bool run(std::vector<ChatMessage> &messages, String &json, String &error, StatusFn status);
    const String &systemPrompt();

    ClaudeClient &claude_;
    String systemPrompt_;
};
