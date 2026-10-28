// "Claude" robot face: big eyes and a mouth that blink, glance around, react to the stylus, fall asleep
// when left alone, and talk (subtitles + moving mouth + optional voice blips) with the mood Claude picks.
//
// The face is drawn into a 1-bit off-screen canvas and copied to the panel in one SPI burst per frame, so
// it animates without flicker and needs only ~11 KB of RAM.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <string>
#include <vector>

enum class FaceAction : uint8_t { None, Back, Compose };
enum class Mood : uint8_t { Neutral, Happy, Sad, Angry, Surprised, Love, Thinking, Sleepy, Wink };

class FaceUi {
public:
    explicit FaceUi(Adafruit_GFX &gfx);

    // Allocates the canvas; false when there is not enough RAM (the face then shows only subtitles).
    bool begin();
    void show();
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }

    FaceAction press(int16_t x, int16_t y);
    void loop(uint32_t nowMs);

    // Waiting for Claude: thinking face, the question as a subtitle (ASCII).
    void setThinking(const String &questionAscii);
    // Claude's reply (UTF-8, may start with a mood tag such as "[happy]").
    void say(const String &utf8Reply);
    // Something went wrong (no Wi-Fi, API error...): sad face, ASCII text.
    void sayError(const String &ascii);

    // "[happy]" -> Mood::Happy; unknown tags are Neutral.
    static Mood moodFromTag(const std::string &tag);
    // The tags Claude may use, for its system prompt.
    static const char *moodTagList();

private:
    struct Pose {
        float lookX = 0, lookY = 0;  // -1..1
        float openL = 1, openR = 1;  // eye opening, 1 = normal
        float mouth = 0;             // 0 = closed (mood shape), 1 = wide open
        bool operator!=(const Pose &o) const;
    };

    void setMood(Mood mood, uint32_t holdMs);
    void react(int16_t x, int16_t y);
    void startSpeech(const std::string &ascii, uint16_t color);
    void updateSpeech(uint32_t nowMs);
    void updateIdle(uint32_t nowMs);
    Pose targetPose(uint32_t nowMs) const;
    void render();
    void drawEye(int16_t cx, int16_t cy, float open, int8_t side);
    void drawMouth(int16_t mx, int16_t my);
    void drawExtras();
    void blit(uint16_t color);
    void drawTopBar();
    void clearSubtitles();
    void drawSubtitleChars(size_t from, size_t to);
    uint16_t moodColor() const;

    Adafruit_GFX &gfx_;
    GFXcanvas1 *canvas_ = nullptr;
    uint16_t *lineBuf_ = nullptr;
    bool visible_ = false;

    Mood mood_ = Mood::Neutral;
    uint32_t moodUntilMs_ = 0;  // back to Neutral after this (0 = keep)
    Pose pose_;
    Pose drawnPose_;
    Mood drawnMood_ = Mood::Neutral;
    uint8_t drawnPhase_ = 0;
    bool forceRender_ = true;
    uint32_t lastFrameMs_ = 0;

    // Idle behaviour.
    uint32_t nextBlinkMs_ = 0;
    uint32_t blinkStartMs_ = 0;
    uint32_t nextGlanceMs_ = 0;
    float glanceX_ = 0, glanceY_ = 0;
    uint32_t lastInteractionMs_ = 0;
    bool asleep_ = false;
    bool thinking_ = false;

    // Speech.
    std::vector<std::string> lines_;
    size_t page_ = 0;
    size_t revealed_ = 0;  // characters of the current page already shown
    uint32_t pageStartMs_ = 0;
    uint32_t pageDoneMs_ = 0;
    bool speaking_ = false;
    uint16_t speechColor_ = 0;
    uint32_t lastBlipMs_ = 0;
    char currentChar_ = ' ';  // last revealed character, shapes the mouth
};
