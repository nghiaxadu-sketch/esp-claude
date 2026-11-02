// "MiniScript": the tiny C-like language Claude writes app logic in (plain C++, unit-tested on a PC).
//
//   count = count + 1; best = max(best, count)
//   if (t > 0 && run) { t -= 1 } else { run = 0; led(255, 0, 0) }
//   label = run ? 'Dung' : 'Chay'
//
// Values are numbers (double) or strings. There are no loops, so every script finishes quickly.
// Templates such as "Con {mmss(t)}" embed expressions in text.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mini_script {

struct Value {
    bool isText = false;
    double num = 0;
    std::string text;

    static Value number(double n);
    static Value string(const std::string &s);

    double toNumber() const;
    std::string toString() const;
    bool truthy() const;
};

// Formats numbers for display: 3 -> "3", 2.5 -> "2.5", 1.23456 -> "1.23".
std::string formatNumber(double n);

// Functions provided by the device (LED, time, HTTP...). Pure helpers (max, mmss, rand...) are built in.
class Host {
public:
    virtual ~Host() = default;
    // Known function name with an acceptable argument count (used to validate scripts before running them).
    virtual bool hasFunction(const std::string &name, size_t argCount) const = 0;
    virtual bool callFunction(const std::string &name, const std::vector<Value> &args, Value &result,
                              std::string &error) = 0;
};

class Interpreter {
public:
    void setHost(Host *host) { host_ = host; }
    void seedRandom(uint32_t seed) { rng_ = seed != 0 ? seed : 1; }

    void setVar(const std::string &name, const Value &value) { vars_[name] = value; }
    bool hasVar(const std::string &name) const { return vars_.count(name) != 0; }
    bool getVar(const std::string &name, Value &out) const;
    const std::map<std::string, Value> &vars() const { return vars_; }
    void clearVars() { vars_.clear(); }

    // Runs statements (assignments, if/else, function calls).
    bool run(const std::string &code, std::string &error);
    // Evaluates one expression.
    bool eval(const std::string &expr, Value &out, std::string &error);
    // Replaces every {expression} in the template with its value. "{{" and "}}" give literal braces.
    bool interpolate(const std::string &tmpl, std::string &out, std::string &error);

    // Validates without side effects: syntax, declared variables, known functions and argument counts.
    bool checkCode(const std::string &code, std::string &error);
    bool checkExpr(const std::string &expr, std::string &error);
    bool checkTemplate(const std::string &tmpl, std::string &error);

    // Built-in function table lookup (also used by Host implementations to avoid name clashes).
    static bool isBuiltin(const std::string &name, size_t argCount);

    uint32_t nextRandom();

private:
    friend class Exec;
    bool templateParts(const std::string &tmpl, bool checking, std::string &out, std::string &error);

    std::map<std::string, Value> vars_;
    Host *host_ = nullptr;
    uint32_t rng_ = 0x12345678;
};

}  // namespace mini_script
