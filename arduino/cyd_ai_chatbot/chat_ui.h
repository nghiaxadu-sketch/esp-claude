// Scrollable chat view (320x240). Typing happens on the shared TouchKeyboard.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <vector>

#include "ui_common.h"

enum class Speaker : uint8_t { User, Claude, System };

enum class ChatAction : uint8_t {
    None,
    Home,       // "Ve": back to the launcher
    Compose,    // "Nhap": open the keyboard
    ClearChat,  // "Xoa"
};

class ChatUi {
public:
    explicit ChatUi(Adafruit_GFX &tft);

    void begin();
    void show();
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }

    // Adds a message to the chat log (UTF-8 is converted to ASCII for the built-in font).
    void addMessage(Speaker who, const String &utf8Text);
    void clearChat();

    // Right side of the header: clock, Wi-Fi state or "Dang nghi...".
    void setStatus(const String &status);
    void setBusy(bool busy);

    // Handles a new press at screen coordinates; returns what the sketch should do.
    ChatAction handleTouch(int16_t x, int16_t y);

private:
    struct ChatLine {
        String text;
        uint16_t color;
    };

    static constexpr size_t kFooterButtons = 5;

    void drawHeader();
    void drawChatBody();
    void drawFooter();
    void scrollBy(int lines);
    void scrollToBottom();
    int maxScroll() const;

    Adafruit_GFX &tft_;
    bool visible_ = false;
    bool busy_ = false;
    String status_;

    std::vector<ChatLine> lines_;
    int scrollTop_ = 0;
    int visibleLines_ = 0;
    size_t charsPerLine_ = 0;
    int lineHeight_ = 0;

    ui::Rect footer_[kFooterButtons];
};
