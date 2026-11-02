// On-screen QWERTY keyboard (320x240) used for chat questions, app descriptions and Wi-Fi passwords.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include "ui_common.h"

enum class KeyboardResult : uint8_t { None, Done, Cancel };

class TouchKeyboard {
public:
    explicit TouchKeyboard(Adafruit_GFX &gfx);

    // Draws the keyboard. `title` and `placeholder` are ASCII.
    void show(const char *title, const String &initial, const char *placeholder, size_t maxChars,
              const char *doneLabel = "Gui");
    KeyboardResult handleTouch(int16_t x, int16_t y);
    const String &text() const { return input_; }

private:
    enum class Mode : uint8_t { Letters, Symbols };
    enum class KeyCode : uint8_t { Char, Shift, Delete, Mode, Space, Done };

    struct Key {
        ui::Rect r;
        KeyCode code;
        char ch;
        const char *label;
    };

    static constexpr size_t kMaxKeys = 40;

    void buildKeys();
    void addKeyRow(const char *chars, int16_t x, int16_t y, int16_t keyW, int16_t keyH);
    void addKey(int16_t x, int16_t y, int16_t w, int16_t h, KeyCode code, char ch, const char *label);
    void drawKeys();
    void drawKey(const Key &key, bool pressed);
    void drawInputBox();

    Adafruit_GFX &gfx_;
    Mode mode_ = Mode::Letters;
    bool shift_ = false;
    String input_;
    String title_;
    const char *placeholder_ = "";
    const char *doneLabel_ = "Gui";
    size_t maxChars_ = 0;
    Key keys_[kMaxKeys];
    size_t keyCount_ = 0;
    ui::Rect cancelButton_;
};
