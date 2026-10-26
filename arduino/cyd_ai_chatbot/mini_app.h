// Runtime for the small apps Claude writes (plain C++, unit-tested on a PC).
//
// An app is one JSON object: variables, timers and a list of screen items (label, button, rect,
// circle, line, bar). Any item field may be an expression; text fields are templates ("{mmss(t)}").
// Scripts (init, timers, on_tap, on_touch) use the MiniScript language from mini_script.h.
// The full format is described to Claude in app_builder.cpp (kAppFormatGuide).
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "mini_json.h"
#include "mini_script.h"

namespace mini_app {

constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr int16_t kFontW = 6;  // built-in 5x7 font cell at text size 1
constexpr int16_t kFontH = 8;

// Drawing surface (Adafruit GFX on the board, a recorder in tests). Text is printable ASCII.
class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
    virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
    virtual void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) = 0;
    virtual void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) = 0;
    virtual void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) = 0;
    virtual void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) = 0;
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) = 0;
    virtual void drawText(int16_t x, int16_t y, const std::string &ascii, uint8_t size, uint16_t color) = 0;
};

// Device services that scripts can call (LED, buzzer, clock, HTTP...).
class Device {
public:
    virtual ~Device() = default;
    virtual uint32_t millis() = 0;
    virtual bool hasFunction(const std::string &name, size_t argCount) const = 0;
    virtual bool callFunction(const std::string &name, const std::vector<mini_script::Value> &args,
                              mini_script::Value &result, std::string &error) = 0;
};

// Converts "#RRGGBB", "RRGGBB", "#RGB" or a color name ("red", "orange"...) to RGB565.
bool parseColor(const std::string &text, uint16_t &out);

class MiniApp : public mini_script::Host {
public:
    MiniApp(Canvas &canvas, Device &device);

    // Parses and checks the app (syntax, variables, functions). Does not draw anything.
    bool load(const std::string &json, std::string &error);
    const std::string &name() const { return name_; }

    // Resets variables, applies saved state, runs "init" and draws the first frame.
    bool start(std::string &error);
    // Runs due timers and redraws what changed. Call often from the main loop.
    void loop();
    // A new press on the screen.
    void touch(int16_t x, int16_t y);
    // Centers of the visible items that react to taps (used to smoke-test apps before saving them).
    std::vector<std::pair<int16_t, int16_t>> tapTargets() const;
    // Forces a full redraw (e.g. after an overlay was shown).
    void redraw();

    bool exitRequested() const { return exitRequested_; }
    // True once after a script called save(); the caller then stores saveState().
    bool takeSaveRequest()
    {
        const bool requested = saveRequested_;
        saveRequested_ = false;
        return requested;
    }
    bool hasError() const { return !runtimeError_.empty(); }
    const std::string &runtimeError() const { return runtimeError_; }

    // Read access to variables (tests, debugging).
    const mini_script::Interpreter &script() const { return interp_; }

    // Values of the variables listed in "persist", as a JSON object (empty when there are none).
    std::string saveState() const;
    // Applied by the next start().
    void setSavedState(const std::string &json) { savedState_ = json; }

    // mini_script::Host
    bool hasFunction(const std::string &name, size_t argCount) const override;
    bool callFunction(const std::string &name, const std::vector<mini_script::Value> &args,
                      mini_script::Value &result, std::string &error) override;

private:
    enum class Kind : uint8_t { Label, Button, Rect, Circle, Line, Bar };
    enum class Align : uint8_t { Left, Center, Right };

    struct Bounds {
        int16_t x = 0, y = 0, w = 0, h = 0;
        bool intersects(const Bounds &o) const;
        bool contains(int16_t px, int16_t py) const;
    };

    // Every property after evaluating expressions; compared between frames to find what changed.
    struct Resolved {
        bool visible = false;
        Bounds box;
        int16_t x = 0, y = 0, w = 0, h = 0, x2 = 0, y2 = 0, radius = 0;
        uint16_t color = 0, color2 = 0;
        uint8_t size = 1;
        bool fill = true;
        Align align = Align::Left;
        double fraction = 0;
        std::vector<std::string> lines;
        bool operator==(const Resolved &o) const;
        bool operator!=(const Resolved &o) const { return !(*this == o); }
    };

    struct Item {
        Kind kind;
        const mini_json::Value *spec;
        Resolved last;
    };

    struct Timer {
        std::string code;
        uint32_t intervalMs;
        uint32_t nextMs;
    };

    bool parseItems(std::string &error);
    bool parseTimers(std::string &error);
    bool checkAll(std::string &error);
    bool checkItem(const Item &item, std::string &error);
    bool checkField(const mini_json::Value &spec, const char *field, char kind, std::string &error);
    void declareVars();

    bool resolve(const Item &item, Resolved &out, std::string &error);
    bool numberField(const mini_json::Value &spec, const char *field, double fallback, double &out,
                     std::string &error);
    bool boolField(const mini_json::Value &spec, const char *field, bool fallback, bool &out, std::string &error);
    bool textField(const mini_json::Value &spec, const char *field, std::string &out, std::string &error);
    bool colorField(const mini_json::Value &spec, const char *field, uint16_t fallback, uint16_t &out,
                    std::string &error);
    void layoutText(const std::string &utf8, Resolved &r, int16_t boxW);

    void render(bool full);
    void drawItem(const Item &item, const Resolved &r);
    void drawErrorBanner();
    bool runCode(const std::string &code, const char *where);
    void fail(const std::string &error);

    Canvas &canvas_;
    Device &device_;
    mini_script::Interpreter interp_;
    mini_json::Value doc_;
    std::string name_;
    std::vector<Item> items_;
    std::vector<Timer> timers_;
    std::vector<std::string> persist_;
    std::string savedState_;
    std::string runtimeError_;
    uint16_t bg_ = 0;
    bool started_ = false;
    bool rendering_ = false;
    bool exitRequested_ = false;
    bool saveRequested_ = false;
    bool needsFullRedraw_ = true;
};

}  // namespace mini_app
