// Host test for the app engine (no board needed):
//   g++ -std=c++17 -Wall -Wextra -Werror -I.. ../mini_json.cpp ../mini_script.cpp ../mini_app.cpp
//       ../text_utils.cpp ../builtin_apps.cpp ../app_validator.cpp mini_app_test.cpp -o mini_app_test && ./mini_app_test
#include <cstdio>
#include <string>
#include <vector>

#include "app_validator.h"
#include "builtin_apps.h"
#include "device_functions.h"
#include "mini_app.h"
#include "mini_json.h"
#include "mini_script.h"

static int failures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

static void expectEq(const std::string &got, const std::string &want, const char *what)
{
    if (got != want) {
        std::printf("FAIL %s\n  got:  \"%s\"\n  want: \"%s\"\n", what, got.c_str(), want.c_str());
        ++failures;
    }
}

// Counts drawing calls and remembers the text that was drawn.
class RecordingCanvas : public mini_app::Canvas {
public:
    int calls = 0;
    std::vector<std::string> texts;

    void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void fillRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void drawRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void fillCircle(int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void drawCircle(int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void drawLine(int16_t, int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void drawText(int16_t, int16_t, const std::string &ascii, uint8_t, uint16_t) override
    {
        ++calls;
        texts.push_back(ascii);
    }
    bool drew(const std::string &text) const
    {
        for (const std::string &t : texts) {
            if (t == text) {
                return true;
            }
        }
        return false;
    }
    void reset()
    {
        calls = 0;
        texts.clear();
    }
};

class FakeDevice : public mini_app::Device {
public:
    uint32_t now = 1000;
    int ledCalls = 0;
    int httpCalls = 0;

    uint32_t millis() override { return now; }
    bool hasFunction(const std::string &name, size_t argCount) const override
    {
        return device_functions::accepts(name, argCount);
    }
    bool callFunction(const std::string &name, const std::vector<mini_script::Value> &, mini_script::Value &result,
                      std::string &) override
    {
        result = mini_script::Value::number(0);
        if (name == "led") {
            ++ledCalls;
        } else if (name == "hour") {
            result = mini_script::Value::number(9);
        } else if (name == "minute") {
            result = mini_script::Value::number(5);
        } else if (name == "weekday") {
            result = mini_script::Value::number(1);
        } else if (name == "http") {
            ++httpCalls;
            result = mini_script::Value::number(31.5);
        }
        return true;
    }
};

static std::string evalText(mini_script::Interpreter &in, const std::string &expr)
{
    mini_script::Value v;
    std::string err;
    if (!in.eval(expr, v, err)) {
        return "ERR " + err;
    }
    return v.toString();
}

static void testJson()
{
    mini_json::Value v;
    std::string err;
    EXPECT(mini_json::parse(R"({"a": [1, 2.5, "x\nà"], "b": {"c": true}, "d": null})", v, err));
    EXPECT(v.get("a") != nullptr && v.get("a")->items.size() == 3);
    expectEq(v.get("a")->items[2].text, "x\n\xc3\xa0", "json unicode escape");
    EXPECT(v.get("b")->get("c")->boolean);
    EXPECT(!mini_json::parse("{\"a\": }", v, err));
    EXPECT(!mini_json::parse("{\"a\": 1} x", v, err));
    expectEq(mini_json::extractObject("Here:\n```json\n{\"a\": \"}\"}\n```"), "{\"a\": \"}\"}", "extract object");
    expectEq(mini_json::quote("a\"b\n"), "\"a\\\"b\\n\"", "quote");
}

static void testScript()
{
    mini_script::Interpreter in;
    std::string err;
    expectEq(evalText(in, "1 + 2 * 3"), "7", "precedence");
    expectEq(evalText(in, "(1 + 2) * 3 % 4"), "1", "parens and modulo");
    expectEq(evalText(in, "10 / 4"), "2.5", "division");
    expectEq(evalText(in, "1 / 0"), "0", "division by zero");
    expectEq(evalText(in, "'a' + 1 + 2"), "a12", "string concat");
    expectEq(evalText(in, "3 > 2 ? 'yes' : 'no'"), "yes", "ternary");
    expectEq(evalText(in, "!0 && (2 >= 2) || 0"), "1", "logic");
    expectEq(evalText(in, "'abc' == 'abc' and 1 != 2"), "1", "word operators");
    expectEq(evalText(in, "mmss(125) + ' ' + hhmmss(3725)"), "02:05 1:02:05", "time format");
    expectEq(evalText(in, "pad(7, 3) + fmt(3.14159, 2)"), "0073.14", "pad and fmt");
    expectEq(evalText(in, "pick(4, 'a', 'b', 'c')"), "b", "pick wraps");
    expectEq(evalText(in, "part('a,b,c', ',', 1) + count('a,b,c', ',')"), "b3", "split helpers");
    expectEq(evalText(in, "max(1, 5, 3) - min(4, 2)"), "3", "min max");

    in.setVar("x", mini_script::Value::number(1));
    in.setVar("s", mini_script::Value::string(""));
    EXPECT(in.run("x += 4; x *= 2; if (x > 5) { s = 'big' } else { s = 'small' } x--", err));
    expectEq(evalText(in, "x"), "9", "compound assignment");
    expectEq(evalText(in, "s"), "big", "if/else");
    EXPECT(in.run("if (x == 1) s = 'one' else if (x == 9) s = 'nine' else s = 'other'", err));
    expectEq(evalText(in, "s"), "nine", "else if");

    for (int i = 0; i < 50; ++i) {
        mini_script::Value v;
        EXPECT(in.eval("rand(1, 6)", v, err) && v.num >= 1 && v.num <= 6);
    }

    std::string out;
    EXPECT(in.interpolate("x={x}, {{literal}}, {s + '!'}", out, err));
    expectEq(out, "x=9, {literal}, nine!", "interpolate");

    EXPECT(!in.checkCode("y = 1 +", err));
    EXPECT(!in.checkExpr("undefined_var + 1", err));
    EXPECT(err.find("undefined_var") != std::string::npos);
    EXPECT(!in.checkCode("nosuchfn(1)", err));
    EXPECT(!in.checkCode("max()", err));
    EXPECT(!in.checkCode("if (x { }", err));
    EXPECT(in.checkCode("if (x > 1) { x = 0 } else { x += 1 } // comment", err));
}

static void testBuiltinApps()
{
    for (size_t i = 0; i < builtin_apps::kAppCount; ++i) {
        RecordingCanvas canvas;
        FakeDevice device;
        mini_app::MiniApp app(canvas, device);
        std::string err;
        const bool loaded = app.load(builtin_apps::kApps[i].json, err);
        if (!loaded) {
            std::printf("FAIL builtin %s: %s\n", builtin_apps::kApps[i].id, err.c_str());
            ++failures;
            continue;
        }
        EXPECT(app.start(err));
        for (int t = 0; t < 30; ++t) {
            device.now += 200;
            app.loop();
            app.touch(160, 120);
        }
        if (app.hasError()) {
            std::printf("FAIL builtin %s runtime: %s\n", builtin_apps::kApps[i].id, app.runtimeError().c_str());
            ++failures;
        }
    }
}

// Answers the weather app's http() calls like the Open-Meteo API (the board returns arrays as JSON text).
class WeatherDevice : public FakeDevice {
public:
    bool callFunction(const std::string &name, const std::vector<mini_script::Value> &args,
                      mini_script::Value &result, std::string &error) override
    {
        if (name == "wifi") {
            result = mini_script::Value::number(1);
            return true;
        }
        if (name != "http") {
            return FakeDevice::callFunction(name, args, result, error);
        }
        ++httpCalls;
        const std::string path = args.size() > 1 ? args[1].toString() : std::string();
        if (path == "current.temperature_2m") {
            result = mini_script::Value::number(29.6);
        } else if (path == "daily.temperature_2m_max") {
            result = mini_script::Value::string("[31.2,32,30.4,29.9,33.1]");
        } else if (path == "daily.temperature_2m_min") {
            result = mini_script::Value::string("[24,25.5,23.1,22,24.8]");
        } else if (path == "daily.weather_code") {
            result = mini_script::Value::string("[61,0,3,95,2]");
        } else {
            result = mini_script::Value::string("");
        }
        return true;
    }
};

static void testWeatherApp()
{
    const char *json = nullptr;
    for (size_t i = 0; i < builtin_apps::kAppCount; ++i) {
        if (std::string(builtin_apps::kApps[i].id) == "thoi_tiet") {
            json = builtin_apps::kApps[i].json;
        }
    }
    EXPECT(json != nullptr);
    if (json == nullptr) {
        return;
    }
    RecordingCanvas canvas;
    WeatherDevice device;
    mini_app::MiniApp app(canvas, device);
    std::string err;
    EXPECT(app.load(json, err));
    EXPECT(app.start(err));
    EXPECT(canvas.drew("Dang tai..."));
    for (int t = 0; t < 5; ++t) {
        device.now += 400;
        app.loop();
    }
    EXPECT(!app.hasError());
    EXPECT(device.httpCalls == 4);
    expectEq(evalText(const_cast<mini_script::Interpreter &>(app.script()), "hi1 + '/' + lo2 + '/' + c3"), "32/23.1/95",
             "weather arrays parsed");
    EXPECT(canvas.drew("30"));
    EXPECT(canvas.drew("Co mua"));
    EXPECT(canvas.drew("Hom nay"));
    EXPECT(canvas.drew("Thu 3"));  // weekday() is Monday in the fake device, so tomorrow is Tuesday
    EXPECT(canvas.drew("24\xF7"));
    EXPECT(canvas.drew("33\xF7"));
}

static void testTimerApp()
{
    RecordingCanvas canvas;
    FakeDevice device;
    mini_app::MiniApp app(canvas, device);
    std::string err;
    EXPECT(app.load(builtin_apps::kExampleForClaude, err));
    app.setSavedState("{\"total\": 120}");
    EXPECT(app.start(err));
    EXPECT(canvas.drew("02:00"));
    EXPECT(canvas.drew("Bat dau"));  // diacritics removed for the built-in font

    canvas.reset();
    app.touch(50, 200);  // "Bat dau"
    EXPECT(canvas.drew("Tam dung"));
    for (int s = 0; s < 3; ++s) {
        device.now += 1000;
        app.loop();
    }
    mini_script::Value t;
    EXPECT(app.script().getVar("t", t) && t.num == 117);

    canvas.reset();
    device.now += 1000;
    app.loop();
    EXPECT(canvas.drew("01:56"));
    EXPECT(canvas.calls < 20);  // only the changed items were redrawn
    EXPECT(!canvas.drew("Lam lai"));

    app.touch(200, 150);  // "15'" button sets total and calls save()
    EXPECT(app.takeSaveRequest());
    expectEq(app.saveState(), "{\"total\":900}", "persist state");
    EXPECT(!app.hasError());
}

static void testErrors()
{
    RecordingCanvas canvas;
    FakeDevice device;
    mini_app::MiniApp app(canvas, device);
    std::string err;

    EXPECT(!app.load("{\"items\": [{\"type\": \"label\", \"text\": \"{nope}\"}]}", err));
    EXPECT(err.find("nope") != std::string::npos);
    EXPECT(!app.load("{\"items\": [{\"type\": \"slider\"}]}", err));
    EXPECT(!app.load("{\"items\": [{\"type\": \"button\", \"onclick\": \"x=1\"}]}", err));
    EXPECT(!app.load("{\"items\": [{\"type\": \"rect\", \"color\": \"blurple\"}]}", err));
    EXPECT(!app.load("{\"vars\": {\"a\": 1}, \"items\": [], \"timers\": [{\"ms\": 100, \"do\": \"a = \"}]}", err));

    // http() inside an item field is refused at runtime (it would block every redraw).
    EXPECT(app.load("{\"items\": [{\"type\": \"label\", \"text\": \"{http('https://x')}\"}]}", err));
    EXPECT(!app.start(err));
    EXPECT(err.find("http") != std::string::npos);

    // ... but works from a timer.
    const char *weather = R"J({"vars": {"temp": ""}, "init": "temp = http('https://api.example/x', 'current.t')",
        "items": [{"type": "label", "x": 10, "y": 10, "text": "{temp} C"}]})J";
    EXPECT(app.load(weather, err));
    EXPECT(app.start(err));
    EXPECT(canvas.drew("31.5 C"));
    EXPECT(device.httpCalls == 1);

    EXPECT(app.load("{\"items\": [{\"type\": \"button\", \"x\": 0, \"y\": 0, \"on_tap\": \"exit()\"}]}", err));
    EXPECT(app.start(err));
    app.touch(5, 5);
    EXPECT(app.exitRequested());
}

static void testValidator()
{
    std::string err;
    for (size_t i = 0; i < builtin_apps::kAppCount; ++i) {
        EXPECT(app_validator::validate(builtin_apps::kApps[i].json, err));
    }
    // Runtime error only reachable after a tap is caught by the dry run.
    EXPECT(!app_validator::validate(
        "{\"vars\": {\"n\": 0}, \"items\": [{\"type\": \"button\", \"on_tap\": \"n = undefinedThing\"}]}", err));
    EXPECT(err.find("undefinedThing") != std::string::npos);
}

int main()
{
    testValidator();
    testJson();
    testScript();
    testBuiltinApps();
    testWeatherApp();
    testTimerApp();
    testErrors();
    if (failures == 0) {
        std::printf("mini_app: all tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
