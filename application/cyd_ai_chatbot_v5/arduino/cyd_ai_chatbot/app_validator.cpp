#include "app_validator.h"

#include "device_functions.h"
#include "mini_app.h"

namespace app_validator {
namespace {

constexpr int kDryRunTicks = 5;
constexpr uint32_t kDryRunStepMs = 1000;
constexpr int16_t kMaxCoord = 2 * mini_app::kScreenW;

class NullCanvas : public mini_app::Canvas {
public:
    void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) override {}
    void drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) override {}
    void fillRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) override {}
    void drawRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) override {}
    void fillCircle(int16_t, int16_t, int16_t, uint16_t) override {}
    void drawCircle(int16_t, int16_t, int16_t, uint16_t) override {}
    void drawLine(int16_t, int16_t, int16_t, int16_t, uint16_t) override {}
    void drawText(int16_t, int16_t, const std::string &, uint8_t, uint16_t) override {}
};

// Accepts every documented function and does nothing (http returns "", time functions return 0).
class DryDevice : public mini_app::Device {
public:
    uint32_t now = 0;
    uint32_t millis() override { return now; }
    bool hasFunction(const std::string &name, size_t argCount) const override
    {
        return device_functions::accepts(name, argCount);
    }
    bool callFunction(const std::string &name, const std::vector<mini_script::Value> &, mini_script::Value &result,
                      std::string &) override
    {
        result = name == "http" ? mini_script::Value::string("") : mini_script::Value::number(0);
        return true;
    }
};

}  // namespace

bool validate(const std::string &json, std::string &error)
{
    NullCanvas canvas;
    DryDevice device;
    mini_app::MiniApp app(canvas, device);
    if (!app.load(json, error)) {
        return false;
    }
    if (!app.start(error)) {
        error = "Loi khi chay thu: " + error;
        return false;
    }
    for (int i = 0; i < kDryRunTicks && !app.hasError(); ++i) {
        device.now += kDryRunStepMs;
        app.loop();
    }
    for (const auto &target : app.tapTargets()) {
        if (app.hasError() || app.exitRequested()) {
            break;
        }
        if (target.first < 0 || target.second < 0 || target.first > kMaxCoord || target.second > kMaxCoord) {
            continue;
        }
        app.touch(target.first, target.second);
        device.now += kDryRunStepMs;
        app.loop();
    }
    if (app.hasError()) {
        error = "Loi khi chay thu: " + app.runtimeError();
        return false;
    }
    return true;
}

}  // namespace app_validator
