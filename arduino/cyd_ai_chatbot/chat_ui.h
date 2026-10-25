// Touch UI for 320x240: a scrollable chat view and an on-screen QWERTY keyboard.
#pragma once

#include <Adafruit_ST7789.h>
#include <Arduino.h>
#include <vector>

enum class Speaker : uint8_t { User, Claude, System };

enum class UiAction : uint8_t {
    None,
    Send,       // input() holds the question
    ClearChat,  // user pressed "Xoa"
};

class ChatUi {
public:
    explicit ChatUi(Adafruit_ST7789 &tft);

    void begin();
    void showChat();
    void showKeyboard();
    bool keyboardVisible() const { return screen_ == Screen::Keyboard; }

    // Adds a message to the chat log (UTF-8 is converted to ASCII for the built-in font).
    void addMessage(Speaker who, const String &utf8Text);
    void clearChat();

    // Right side of the header: clock, Wi-Fi state or "Dang nghi...".
    void setStatus(const String &status);
    void setBusy(bool busy);

    // Handles a new press at screen coordinates; returns what the sketch should do.
    UiAction handleTouch(int16_t x, int16_t y);

    const String &input() const { return input_; }
    void clearInput();

    // Full-screen message used while booting / calibrating.
    void drawSplash(const char *title, const char *line1, const char *line2 = nullptr);

private:
    enum class Screen : uint8_t { Chat, Keyboard };
    enum class KeyboardMode : uint8_t { Letters, Symbols };
    enum class KeyCode : uint8_t { Char, Shift, Delete, Mode, Space, Send };

    struct ChatLine {
        String text;
        uint16_t color;
    };

    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    };

    struct Key {
        Rect r;
        KeyCode code;
        char ch;
        const char *label;
    };

    static constexpr size_t kMaxKeys = 40;
    static constexpr size_t kFooterButtons = 4;

    void drawHeader(const char *title);
    void drawChatBody();
    void drawFooter();
    void drawButton(const Rect &r, const char *label, uint16_t bg, uint16_t fg, uint8_t textSize = 2);
    void drawInputBox();
    void drawKeys();
    void drawKey(const Key &key, bool pressed);
    void buildKeys();
    void addKeyRow(const char *chars, int16_t x, int16_t y, int16_t keyW, int16_t keyH);
    void addKey(int16_t x, int16_t y, int16_t w, int16_t h, KeyCode code, char ch, const char *label);
    UiAction handleChatTouch(int16_t x, int16_t y);
    UiAction handleKeyboardTouch(int16_t x, int16_t y);
    void scrollBy(int lines);
    void scrollToBottom();
    int maxScroll() const;

    Adafruit_ST7789 &tft_;
    Screen screen_ = Screen::Chat;
    KeyboardMode mode_ = KeyboardMode::Letters;
    bool shift_ = false;
    bool busy_ = false;
    String status_;
    String input_;

    std::vector<ChatLine> lines_;
    int scrollTop_ = 0;
    int visibleLines_ = 0;
    size_t charsPerLine_ = 0;
    int lineHeight_ = 0;

    Key keys_[kMaxKeys];
    size_t keyCount_ = 0;
    Rect footer_[kFooterButtons];
    Rect cancelButton_;
};
