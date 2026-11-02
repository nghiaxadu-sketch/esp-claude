// Checks an app before it is saved: parse + static checks, then a short dry run (init, a few timer
// ticks and a tap on every button) with a device that has no side effects.
// Plain C++ so the same check runs in the host tests.
#pragma once

#include <string>

namespace app_validator {

// Returns false with a message Claude can act on (it is sent back to Claude to fix the app).
bool validate(const std::string &json, std::string &error);

}  // namespace app_validator
