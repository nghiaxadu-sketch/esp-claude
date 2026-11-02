// AI roles ("Vai tro AI") for Chat and the Claude face: teachers, translators, quiz-show hosts, doctors,
// foreign friends... Each role adds a short instruction to the system prompt. Also the reply language
// setting (automatic / Vietnamese / English / Thai). Plain C++ data, no Arduino headers.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace personas {

struct Persona {
    const char *name;    // ASCII, shown in Settings
    const char *prompt;  // appended to the system prompt; "" for the default assistant
};

size_t count();
// Index is clamped; 0 is the default "Lily than thien" style assistant.
const Persona &get(size_t index);

enum class Language : uint8_t { Auto, Vietnamese, English, Thai };
constexpr uint8_t kLanguageCount = 4;
const char *languageName(Language lang);  // "Tu dong", "Tieng Viet", "English", "Thai"
// Instruction added to the system prompt ("" for Auto).
const char *languagePrompt(Language lang);

}  // namespace personas
