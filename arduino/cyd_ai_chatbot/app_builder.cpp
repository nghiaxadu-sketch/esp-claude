#include "app_builder.h"

#include "app_validator.h"
#include "board_hw.h"
#include "board_pins.h"
#include "builtin_apps.h"
#include "config.h"
#include "device_functions.h"
#include "mini_json.h"

namespace {

// How apps are written. The device functions, board notes and the example are appended at runtime.
constexpr const char *kAppFormatGuide = R"GUIDE(You write small apps for an ESP32 board with a 2.8" 320x240 landscape resistive touch screen (taps only, no swipe or drag) and an RGB LED. The app is ONE JSON object that the device interprets. Reply with ONLY that JSON object: no prose, no Markdown fences.

Top-level fields:
- "name": short app name in Vietnamese (max 16 chars). "color": launcher tile color. "bg": background color.
- "vars": {"name": initial value} for EVERY variable used anywhere (numbers or strings; true/false become 1/0). tx, ty (last tap position) are predefined.
- "persist": ["var", ...] stored in flash when save() runs and when the app closes, restored at the next start before "init".
- "init": script run once at start.
- "tick": ms + "on_tick": script, and/or "timers": [{"ms": 500, "do": "script"}] (max 8 timers, min 50 ms).
- "on_touch": script for a tap that hits no item with "on_tap".
- "items": drawn in order (later items on top). After every timer/tap the screen is redrawn; only changed items are repainted.
  label  {x, y, text, size 1-8 (default 2), color, w, align "left"|"center"|"right"} - with w the text wraps inside w and align applies
  button {x, y, w, h, text, size, color (background), text_color, radius, on_tap}
  rect   {x, y, w, h, color, fill (default true), radius}
  circle {x, y, r, color, fill}
  line   {x, y, x2, y2, color}
  bar    {x, y, w, h, value, max, color, bg}  progress bar
  Every item also takes "show" (expression; hidden when false) and "on_tap" (script). Other field names are rejected.
- Number fields may be expressions as strings ("x": "20 + i * 30"). Text fields are templates: "Diem: {score}". Colors: "#RRGGBB", a name (white black red green blue yellow orange purple pink cyan teal gray darkgray brown lime navy gold magenta) or a template "{hot ? 'red' : 'blue'}".

Text: built-in font, each char is 6x8 px times size (size 2 = 12x16, size 4 = 24x32), so a line holds 320/(6*size) chars. Vietnamese diacritics are displayed without marks; still write proper Vietnamese.

Script language: statements `a = expr`, `a += 1` (also -= *= /= %=, ++, --), `if (cond) { ... } else if (cond) { ... } else { ... }` and function calls, separated by ';'. No loops, arrays or user functions. Operators: + - * / % (x/0 = 0), == != < <= > >=, && || ! (or and or not), cond ? a : b. '+' joins text when either side is a string. Use single-quoted strings inside JSON: 'text'.
Built-in functions: abs min max floor ceil round(x[,digits]) int sqrt pow sin cos clamp(x,lo,hi) rand(lo,hi) (integer, inclusive) str num len sub(s,start[,len]) upper lower pad(n,width) (zero pad) fmt(x,decimals) mmss(sec) hhmmss(sec) pick(i,a,b,c...) (0-based, wraps; use it instead of arrays) count(s,sep) part(s,sep,i) contains(s,sub) repeat(s,n), exit() closes the app, save() stores the persist vars.
)GUIDE";

constexpr const char *kAppRules = R"RULES(
Rules:
- Declare every variable in "vars". Use only the functions and item types listed above.
- Keep everything inside x 0..319, y 0..239. Touch targets at least 44x36 px with 6 px gaps (the touch panel is imprecise).
- The user leaves an app by holding the screen for 1.5 s, so no exit button is needed.
- Keep the JSON under 5000 characters; reuse items with "show" rather than many copies.
- Live data: call http() with a free JSON API that needs no key, in "init" and in a timer of at least 60000 ms, store the value in a var and show a loading text until it arrives.
- All text the user sees is Vietnamese.
)RULES";

constexpr const char *kCreatePrefix = "Viet app nay (yeu cau cua nguoi dung, co the go tieng Viet khong dau):\n";
constexpr const char *kEditFormat =
    "App hien tai:\n%s\n\n%sYeu cau sua (tieng Viet, co the khong dau): %s\n\n"
    "Tra ve TOAN BO JSON da sua, chi JSON.";
constexpr const char *kRepairFormat =
    "May bao loi khi kiem tra app: %s\nSua loi va tra ve TOAN BO JSON da sua, chi JSON.";

String deviceNotes()
{
    String notes = "Device functions:\n";
    for (const device_functions::Info &f : device_functions::kFunctions) {
        notes += "- ";
        notes += f.help;
        notes += "\n";
    }
    notes += "This board: ";
    notes += CYD_BOARD_NAME;
    notes += board_hw::hasSpeaker() ? " (has a speaker, beep() works)" : " (no speaker: beep() does nothing)";
    notes += CYD_PIN_LIGHT_SENSOR >= 0 ? ", has a light sensor.\n" : ", no light sensor.\n";
    notes += "User location for weather and similar: ";
    notes += APP_LOCATION_TEXT;
    notes += ". Clock is local time (UTC+7).\n";
    return notes;
}

String formatMessage(const char *format, const String &a, const String &b = String(), const String &c = String())
{
    // Arguments are inserted verbatim: a "%s" inside an app's JSON must not be replaced again.
    String out = format;
    int from = 0;
    for (const String *arg : {&a, &b, &c}) {
        const int at = out.indexOf("%s", from);
        if (at < 0) {
            break;
        }
        out = out.substring(0, at) + *arg + out.substring(at + 2);
        from = at + arg->length();
    }
    return out;
}

}  // namespace

const String &AppBuilder::systemPrompt()
{
    if (systemPrompt_.length() == 0) {
        systemPrompt_ = kAppFormatGuide;
        systemPrompt_ += deviceNotes();
        systemPrompt_ += kAppRules;
        systemPrompt_ += "\nExample app (countdown timer):\n";
        systemPrompt_ += builtin_apps::kExampleForClaude;
        systemPrompt_ += "\n";
    }
    return systemPrompt_;
}

bool AppBuilder::run(std::vector<ChatMessage> &messages, String &json, String &error, StatusFn status)
{
    for (int round = 0; round <= APP_REPAIR_ROUNDS; ++round) {
        const String step =
            round == 0 ? String("Claude dang viet app") : String("Claude dang sua loi (lan ") + String(round) + ")";
        status(step + "...");
        String reply;
        const bool ok = claude_.ask(systemPrompt().c_str(), messages, CLAUDE_APP_MAX_TOKENS, CLAUDE_APP_EFFORT, reply,
                                    error, [&](size_t chars) { status(step + ": " + String(chars) + " ky tu"); });
        if (!ok) {
            return false;
        }
        const std::string candidate = mini_json::extractObject(reply.c_str());
        std::string problem;
        if (candidate.empty()) {
            problem = "khong tim thay object JSON trong cau tra loi";
        } else if (app_validator::validate(candidate, problem)) {
            json = candidate.c_str();
            Serial.printf("[builder] app ok after %d repair round(s), %u bytes\n", round, (unsigned)json.length());
            return true;
        }
        Serial.printf("[builder] rejected: %s\n", problem.c_str());
        error = String("App loi: ") + problem.c_str();
        // Show Claude its own answer and the problem, then ask again.
        messages.push_back({ChatRole::Assistant, candidate.empty() ? reply : String(candidate.c_str())});
        messages.push_back({ChatRole::User, formatMessage(kRepairFormat, problem.c_str())});
    }
    return false;
}

bool AppBuilder::create(const String &description, String &json, String &error, StatusFn status)
{
    std::vector<ChatMessage> messages = {{ChatRole::User, String(kCreatePrefix) + description}};
    return run(messages, json, error, status);
}

bool AppBuilder::edit(const String &currentJson, const String &request, const String &lastError, String &json,
                      String &error, StatusFn status)
{
    const String errorNote = lastError.length() > 0 ? "Loi luc chay gan nhat: " + lastError + "\n\n" : String();
    std::vector<ChatMessage> messages = {{ChatRole::User, formatMessage(kEditFormat, currentJson, errorNote, request)}};
    return run(messages, json, error, status);
}
