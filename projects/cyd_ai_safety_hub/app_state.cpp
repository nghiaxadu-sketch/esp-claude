#include "app_state.h"

Snapshot AppState::snap;
safety::Thresholds AppState::th;
History AppState::hist;
AiView AppState::ai;
DeviceInfo AppState::dev;
SemaphoreHandle_t AppState::mutex_ = nullptr;

void AppState::begin() {
    if (!mutex_) mutex_ = xSemaphoreCreateRecursiveMutex();
    for (int i = 0; i < HISTORY_LEN; ++i) {
        hist.co2[i] = -1;
        hist.chargerTempX10[i] = INT16_MIN;
    }
}

void AppState::lock() { xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }

void AppState::unlock() { xSemaphoreGiveRecursive(mutex_); }

void AppState::pushHistory(float co2, float chargerTempC) {
    hist.co2[hist.head] = isnan(co2) ? -1 : static_cast<int16_t>(constrain(co2, 0.0f, 32000.0f));
    hist.chargerTempX10[hist.head] =
        isnan(chargerTempC) ? INT16_MIN : static_cast<int16_t>(constrain(chargerTempC * 10.0f, -3000.0f, 3000.0f));
    hist.head = (hist.head + 1) % HISTORY_LEN;
    if (hist.count < HISTORY_LEN) ++hist.count;
}
