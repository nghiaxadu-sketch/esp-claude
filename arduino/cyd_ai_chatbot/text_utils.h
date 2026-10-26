// Text helpers for the 320x240 chat screen.
// Plain C++ (no Arduino types) so they can be unit-tested on a PC: see test/text_utils_test.cpp.
#pragma once

#include <string>
#include <vector>

namespace text_utils {

// Converts UTF-8 to printable ASCII for the built-in Adafruit GFX font.
// Vietnamese letters lose their diacritics ("Xin chào, đây" -> "Xin chao, day"),
// typographic quotes/dashes become ASCII, emoji and other symbols are dropped or become '?'.
std::string utf8ToAscii(const std::string &utf8);

// Removes the Markdown markers Claude sometimes still uses (**bold**, __bold__, `code`, "# " headings).
std::string stripMarkdown(const std::string &text);

// Word-wraps ASCII text to lines of at most maxChars characters.
// '\n' starts a new line, words longer than maxChars are split, blank lines are kept.
std::vector<std::string> wrapText(const std::string &text, size_t maxChars);

}  // namespace text_utils
