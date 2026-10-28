// Tiny JSON DOM used by the app runtime (plain C++, no Arduino types, unit-tested on a PC).
// ArduinoJson is still used for the Claude API; this one keeps the app engine testable without it.
#pragma once

#include <string>
#include <vector>

namespace mini_json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string text;
    // Array elements, or object values (keys[i] names items[i]).
    std::vector<Value> items;
    std::vector<std::string> keys;

    bool isNull() const { return type == Type::Null; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }
    bool isBool() const { return type == Type::Bool; }

    // Object member lookup; nullptr when missing or not an object.
    const Value *get(const std::string &key) const;
    // Helpers with defaults for optional fields.
    std::string getString(const std::string &key, const std::string &fallback = std::string()) const;
    double getNumber(const std::string &key, double fallback) const;
};

// Parses a complete JSON document. On failure returns false and describes the problem in `error`.
bool parse(const std::string &text, Value &out, std::string &error);

// Returns the outermost {...} object found in text (Claude may wrap JSON in prose or ``` fences).
// Returns an empty string when there is none.
std::string extractObject(const std::string &text);

// Quotes and escapes a string for writing JSON.
std::string quote(const std::string &text);

}  // namespace mini_json
