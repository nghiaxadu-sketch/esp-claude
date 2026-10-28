#include "claude_async.h"

namespace {

constexpr uint32_t kTaskStackBytes = 12288;  // TLS handshake + streaming parser
constexpr UBaseType_t kTaskPriority = 1;     // same as the Arduino loop
constexpr uint32_t kWaitPollMs = 20;

}  // namespace

bool ClaudeAsync::start(const char *systemPrompt, const std::vector<ChatMessage> &history, uint32_t maxTokens,
                        const char *effort)
{
    if (state_.load() != State::Idle) {
        return false;
    }
    systemPrompt_ = systemPrompt;  // the prompts are string literals that live forever
    history_ = history;
    maxTokens_ = maxTokens;
    effort_ = effort;
    state_.store(State::Running);
    if (xTaskCreate(taskMain, "claude_ask", kTaskStackBytes, this, kTaskPriority, nullptr) != pdPASS) {
        state_.store(State::Idle);
        return false;
    }
    return true;
}

void ClaudeAsync::taskMain(void *arg)
{
    ClaudeAsync *self = static_cast<ClaudeAsync *>(arg);
    String reply, error;
    const bool ok =
        self->client_.ask(self->systemPrompt_, self->history_, self->maxTokens_, self->effort_, reply, error);
    self->ok_ = ok;
    self->reply_ = reply;
    self->error_ = error;
    self->history_.clear();
    self->state_.store(State::Done);  // publishes the results to the main loop
    vTaskDelete(nullptr);
}

bool ClaudeAsync::takeResult(bool &ok, String &reply, String &error)
{
    if (state_.load() != State::Done) {
        return false;
    }
    ok = ok_;
    reply = reply_;
    error = error_;
    reply_ = String();
    error_ = String();
    state_.store(State::Idle);
    return true;
}

void ClaudeAsync::waitIdle()
{
    while (state_.load() == State::Running) {
        delay(kWaitPollMs);
    }
}
