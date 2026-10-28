// Runs one ClaudeClient::ask() at a time on a background FreeRTOS task, so the screen keeps animating
// (the AI face blinks and "thinks") while the HTTPS request is in flight.
#pragma once

#include <Arduino.h>
#include <atomic>
#include <vector>

#include "claude_client.h"

class ClaudeAsync {
public:
    explicit ClaudeAsync(ClaudeClient &client) : client_(client) {}

    // Copies the request and starts the task; false when one is still running or the task cannot start.
    bool start(const char *systemPrompt, const std::vector<ChatMessage> &history, uint32_t maxTokens,
               const char *effort);
    bool busy() const { return state_.load() != State::Idle; }
    // True once per finished request; then the next start() is allowed.
    bool takeResult(bool &ok, String &reply, String &error);
    // Blocks until a running request has finished (before using the client from the main loop).
    void waitIdle();

private:
    enum class State : uint8_t { Idle, Running, Done };
    static void taskMain(void *arg);

    ClaudeClient &client_;
    const char *systemPrompt_ = "";
    std::vector<ChatMessage> history_;
    uint32_t maxTokens_ = 0;
    const char *effort_ = "";
    bool ok_ = false;
    String reply_;
    String error_;
    std::atomic<State> state_{State::Idle};
};
