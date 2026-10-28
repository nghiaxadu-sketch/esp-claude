#include "face_ui.h"

#include <math.h>

#include "board_hw.h"
#include "config.h"
#include "text_utils.h"
#include "ui_common.h"

namespace {

using namespace ui;

// Screen layout.
constexpr int16_t kFaceTop = 26;
constexpr int16_t kFaceW = kScreenW;
constexpr int16_t kFaceH = 146;
constexpr int16_t kSubTop = kFaceTop + kFaceH + 2;
constexpr int16_t kSubX = 10;
constexpr int16_t kSubLineH = 20;
constexpr size_t kSubLines = 3;
constexpr size_t kSubChars = 25;  // size-2 characters per line
constexpr uint8_t kSubSize = 2;
constexpr Rect kComposeButton = {222, 1, 94, 22};
constexpr int16_t kChunkRows = 8;  // rows converted per SPI burst

// Face geometry inside the canvas.
constexpr int16_t kEyeW = 60;
constexpr int16_t kEyeH = 72;
constexpr int16_t kEyeR = 18;
constexpr int16_t kEyeDX = 64;
constexpr int16_t kEyeY = 62;
constexpr int16_t kLookX = 16;
constexpr int16_t kLookY = 10;
constexpr int16_t kMouthY = 126;
constexpr int16_t kClosedEyeH = 8;

// Timing.
constexpr uint32_t kFrameMs = 33;
constexpr uint32_t kBlinkMs = 90;  // closing half of a blink
constexpr uint32_t kCharsPerSec = 26;
constexpr uint32_t kPageHoldMs = 1400;
constexpr uint32_t kPageHoldPerCharMs = 30;
constexpr uint32_t kMoodHoldMs = 6000;
constexpr uint32_t kReactMs = 1600;
constexpr uint32_t kDeepSleepExtraMs = 30000;
constexpr uint32_t kPhaseMs = 400;
constexpr uint32_t kBlipGapMs = 70;
constexpr float kFollow = 0.4f;
constexpr float kMouthFollow = 0.6f;
constexpr float kPoseEpsilon = 0.02f;

const char *const kTapPhrases[] = {"Hihi, nhot qua!", "Minh o day ne!", "Cham nhe thoi nha.",
                                   "Ban can minh giup gi khong?"};
constexpr const char *kEyePokePhrase = "Ui! Dung choc vao mat minh ma!";
constexpr const char *kWakePhrase = "Oa... minh day roi day!";

constexpr uint16_t kColorEyes = rgb565(0x4F, 0xD8, 0xFF);
constexpr uint16_t kColorLove = rgb565(0xFF, 0x6F, 0xA8);
constexpr uint16_t kColorAngry = rgb565(0xFF, 0x4A, 0x3A);
constexpr uint16_t kColorSad = rgb565(0x5A, 0x8C, 0xFF);
constexpr uint16_t kColorSleepy = rgb565(0x2E, 0x8F, 0xA8);

bool isVowel(char c)
{
    switch (c | 0x20) {
    case 'a': case 'e': case 'i': case 'o': case 'u': case 'y': return true;
    default: return false;
    }
}

bool isLetter(char c)
{
    const char lower = (char)(c | 0x20);
    return lower >= 'a' && lower <= 'z';
}

}  // namespace

bool FaceUi::Pose::operator!=(const Pose &o) const
{
    return fabsf(lookX - o.lookX) > kPoseEpsilon || fabsf(lookY - o.lookY) > kPoseEpsilon ||
           fabsf(openL - o.openL) > kPoseEpsilon || fabsf(openR - o.openR) > kPoseEpsilon ||
           fabsf(mouth - o.mouth) > kPoseEpsilon;
}

FaceUi::FaceUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
}

bool FaceUi::begin()
{
    canvas_ = new GFXcanvas1(kFaceW, kFaceH);
    lineBuf_ = static_cast<uint16_t *>(malloc(sizeof(uint16_t) * kFaceW * kChunkRows));
    if (canvas_ == nullptr || canvas_->getBuffer() == nullptr || lineBuf_ == nullptr) {
        Serial.println("[face] not enough RAM for the face canvas");
        delete canvas_;
        canvas_ = nullptr;
        free(lineBuf_);
        lineBuf_ = nullptr;
        return false;
    }
    lastInteractionMs_ = millis();
    return true;
}

Mood FaceUi::moodFromTag(const std::string &tag)
{
    static const struct {
        const char *tag;
        Mood mood;
    } kTags[] = {{"happy", Mood::Happy},         {"sad", Mood::Sad},           {"angry", Mood::Angry},
                 {"surprised", Mood::Surprised}, {"love", Mood::Love},         {"thinking", Mood::Thinking},
                 {"sleepy", Mood::Sleepy},       {"wink", Mood::Wink},         {"neutral", Mood::Neutral}};
    for (const auto &t : kTags) {
        if (tag == t.tag) {
            return t.mood;
        }
    }
    return Mood::Neutral;
}

const char *FaceUi::moodTagList()
{
    return "[happy] [sad] [angry] [surprised] [love] [thinking] [sleepy] [wink] [neutral]";
}

// ---------------------------------------------------------------- screen

void FaceUi::drawTopBar()
{
    gfx_.fillRect(0, 0, kScreenW, kFaceTop, kColorBlack);
    drawChevronLeft(gfx_, 8, 4, 7, kColorWhite);
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(28, 9);
    gfx_.print("Claude");
    drawButton(gfx_, kComposeButton, "Noi chuyen", kColorAccent, kColorWhite, 1);
}

void FaceUi::show()
{
    visible_ = true;
    gfx_.fillScreen(kColorBlack);
    drawTopBar();
    forceRender_ = true;
    clearSubtitles();
    if (speaking_) {
        drawSubtitleChars(0, revealed_);
    } else if (thinking_) {
        setThinking(String(lines_.empty() ? "" : lines_.front().c_str()));
    }
    lastInteractionMs_ = millis();
}

void FaceUi::clearSubtitles()
{
    gfx_.fillRect(0, kSubTop, kScreenW, kScreenH - kSubTop, kColorBlack);
    if (!speaking_ && !thinking_) {
        drawCentered(gfx_, "Cham \"Noi chuyen\" de tro chuyen", 0, kScreenW, kSubTop + 24, 1, kColorMuted);
    }
}

void FaceUi::drawSubtitleChars(size_t from, size_t to)
{
    if (!visible_) {
        return;
    }
    gfx_.setTextSize(kSubSize);
    gfx_.setTextColor(speechColor_);
    size_t index = 0;
    const size_t first = page_ * kSubLines;
    for (size_t l = first; l < lines_.size() && l < first + kSubLines; ++l) {
        const std::string &line = lines_[l];
        for (size_t c = 0; c < line.size(); ++c, ++index) {
            if (index >= from && index < to) {
                gfx_.setCursor(kSubX + (int16_t)c * kFontW * kSubSize, kSubTop + 4 + (int16_t)(l - first) * kSubLineH);
                gfx_.print(line[c]);
            }
        }
    }
}

// ---------------------------------------------------------------- moods and speech

void FaceUi::setMood(Mood mood, uint32_t holdMs)
{
    mood_ = mood;
    moodUntilMs_ = holdMs > 0 ? millis() + holdMs : 0;
}

void FaceUi::setThinking(const String &questionAscii)
{
    thinking_ = true;
    speaking_ = false;
    asleep_ = false;
    setMood(Mood::Thinking, 0);
    lines_ = {std::string(questionAscii.c_str())};
    if (!visible_) {
        return;
    }
    clearSubtitles();
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorUser);
    const std::vector<std::string> wrapped = text_utils::wrapText("Ban: " + lines_.front(), 50);
    for (size_t i = 0; i < wrapped.size() && i < 2; ++i) {
        gfx_.setCursor(kSubX, kSubTop + 4 + (int16_t)i * 11);
        gfx_.print(wrapped[i].c_str());
    }
    gfx_.setTextSize(2);
    gfx_.setTextColor(kColorSystem);
    gfx_.setCursor(kSubX, kSubTop + 30);
    gfx_.print("Claude dang nghi");
}

void FaceUi::startSpeech(const std::string &ascii, uint16_t color)
{
    lines_.clear();
    for (const std::string &line : text_utils::wrapText(ascii, kSubChars)) {
        if (!line.empty()) {
            lines_.push_back(line);
        }
    }
    page_ = 0;
    revealed_ = 0;
    pageStartMs_ = millis();
    speechColor_ = color;
    speaking_ = !lines_.empty();
    thinking_ = false;
    asleep_ = false;
    lastInteractionMs_ = millis();
    clearSubtitles();
}

void FaceUi::say(const String &utf8Reply)
{
    std::string text = text_utils::utf8ToAscii(utf8Reply.c_str());
    const std::string tag = text_utils::takeMoodTag(text);
    text = text_utils::stripMarkdown(text);
    setMood(moodFromTag(tag), 0);
    startSpeech(text, kColorText);
}

void FaceUi::sayError(const String &ascii)
{
    setMood(Mood::Sad, 0);
    startSpeech(ascii.c_str(), kColorSystem);
}

void FaceUi::react(int16_t x, int16_t y)
{
    if (asleep_) {
        asleep_ = false;
        setMood(Mood::Surprised, kReactMs);
        board_hw::beep(600, 60);
        if (!speaking_ && !thinking_) {
            startSpeech(kWakePhrase, kColorText);
        }
        return;
    }
    if (thinking_) {
        return;
    }
    const int16_t eyeTop = kFaceTop + kEyeY - kEyeH / 2, eyeBottom = kFaceTop + kEyeY + kEyeH / 2;
    const bool onEye = y >= eyeTop && y <= eyeBottom &&
                       (abs(x - (kScreenW / 2 - kEyeDX)) <= kEyeW / 2 || abs(x - (kScreenW / 2 + kEyeDX)) <= kEyeW / 2);
    if (onEye) {
        setMood(Mood::Angry, kReactMs);
        board_hw::beep(300, 120);
        if (!speaking_) {
            startSpeech(kEyePokePhrase, kColorText);
        }
        return;
    }
    static const Mood kReactions[] = {Mood::Happy, Mood::Love, Mood::Wink, Mood::Surprised};
    setMood(kReactions[random(sizeof(kReactions) / sizeof(kReactions[0]))], kReactMs);
    board_hw::beep(1200 + random(600), 50);
    if (!speaking_ && random(3) == 0) {
        startSpeech(kTapPhrases[random(sizeof(kTapPhrases) / sizeof(kTapPhrases[0]))], kColorText);
    }
}

void FaceUi::updateSpeech(uint32_t nowMs)
{
    if (!speaking_) {
        return;
    }
    size_t pageChars = 0;
    const size_t first = page_ * kSubLines;
    for (size_t l = first; l < lines_.size() && l < first + kSubLines; ++l) {
        pageChars += lines_[l].size();
    }
    const size_t target = min<size_t>(pageChars, (size_t)((nowMs - pageStartMs_) * kCharsPerSec / 1000));
    if (target > revealed_) {
        drawSubtitleChars(revealed_, target);
        revealed_ = target;
        if (revealed_ == pageChars) {
            pageDoneMs_ = nowMs;
        }
        // Voice blip on letters, like a little robot talking.
        size_t index = 0;
        char current = ' ';
        for (size_t l = first; l < lines_.size() && l < first + kSubLines && index < revealed_; ++l) {
            const std::string &line = lines_[l];
            if (revealed_ - index <= line.size()) {
                current = line[revealed_ - index - 1];
            }
            index += line.size();
        }
        currentChar_ = current;
        if (FACE_VOICE_BLIPS && isLetter(current) && nowMs - lastBlipMs_ >= kBlipGapMs && visible_) {
            board_hw::beep(isVowel(current) ? 520 + random(260) : 380 + random(160), 28);
            lastBlipMs_ = nowMs;
        }
    }
    if (revealed_ < pageChars) {
        return;
    }
    currentChar_ = ' ';
    const uint32_t hold = kPageHoldMs + (uint32_t)pageChars * kPageHoldPerCharMs;
    if (nowMs - pageDoneMs_ < hold) {
        return;
    }
    if (first + kSubLines < lines_.size()) {
        ++page_;
        revealed_ = 0;
        pageStartMs_ = nowMs;
        if (visible_) {
            gfx_.fillRect(0, kSubTop, kScreenW, kScreenH - kSubTop, kColorBlack);
        }
        return;
    }
    speaking_ = false;
    if (mood_ != Mood::Neutral && moodUntilMs_ == 0) {
        moodUntilMs_ = nowMs + kMoodHoldMs;
    }
    lastInteractionMs_ = nowMs;
}

void FaceUi::updateIdle(uint32_t nowMs)
{
    if (moodUntilMs_ != 0 && (int32_t)(nowMs - moodUntilMs_) >= 0) {
        moodUntilMs_ = 0;
        mood_ = Mood::Neutral;
    }
    if ((int32_t)(nowMs - nextBlinkMs_) >= 0) {
        blinkStartMs_ = nowMs;
        nextBlinkMs_ = nowMs + (random(5) == 0 ? 250 : random(2000, 5500));  // sometimes a double blink
    }
    if ((int32_t)(nowMs - nextGlanceMs_) >= 0) {
        const bool center = (glanceX_ != 0 || glanceY_ != 0) && random(2) == 0;
        glanceX_ = center ? 0 : random(-80, 81) / 100.0f;
        glanceY_ = center ? 0 : random(-50, 51) / 100.0f;
        nextGlanceMs_ = nowMs + random(1500, 5000);
    }
    if (speaking_ || thinking_ || asleep_) {
        return;
    }
    const uint32_t idle = nowMs - lastInteractionMs_;
    if (idle > (uint32_t)FACE_SLEEP_AFTER_MS + kDeepSleepExtraMs) {
        asleep_ = true;
        setMood(Mood::Sleepy, 0);
    } else if (idle > (uint32_t)FACE_SLEEP_AFTER_MS && mood_ == Mood::Neutral) {
        setMood(Mood::Sleepy, 0);
    }
}

FaceUi::Pose FaceUi::targetPose(uint32_t nowMs) const
{
    Pose p;
    p.lookX = glanceX_;
    p.lookY = glanceY_;
    switch (mood_) {
    case Mood::Thinking: p.lookX = 0.75f; p.lookY = -0.85f; p.openL = 0.8f; break;
    case Mood::Sad: p.lookX = 0; p.lookY = 0.6f; p.openL = p.openR = 0.85f; break;
    case Mood::Happy: p.openL = p.openR = 0.62f; break;
    case Mood::Wink: p.openL = 0.62f; p.openR = 0.05f; break;
    case Mood::Surprised: p.openL = p.openR = 1.15f; p.lookX = p.lookY = 0; break;
    case Mood::Angry: p.openL = p.openR = 0.85f; p.lookX = 0; break;
    case Mood::Sleepy: p.openL = p.openR = 0.5f; p.lookY = 0.3f; break;
    default: break;
    }
    if (asleep_) {
        p.openL = p.openR = 0.05f;
        p.lookX = 0;
        p.lookY = 0.4f;
    }
    if (speaking_ && currentChar_ != ' ') {
        const float wobble = (sinf(nowMs * 0.045f) + 1) * 0.12f;
        p.mouth = isVowel(currentChar_) ? 0.75f + wobble : (isLetter(currentChar_) ? 0.35f + wobble : 0.08f);
    }
    return p;
}

// ---------------------------------------------------------------- drawing

uint16_t FaceUi::moodColor() const
{
    if (asleep_) {
        return kColorSleepy;
    }
    switch (mood_) {
    case Mood::Love: return kColorLove;
    case Mood::Angry: return kColorAngry;
    case Mood::Sad: return kColorSad;
    case Mood::Sleepy: return kColorSleepy;
    default: return kColorEyes;
    }
}

void FaceUi::drawEye(int16_t cx, int16_t cy, float open, int8_t side)
{
    GFXcanvas1 &c = *canvas_;
    const int16_t w = mood_ == Mood::Surprised ? kEyeW + 6 : kEyeW;
    const int16_t h = (int16_t)max(4.0f, kEyeH * open);
    if (asleep_ || h <= kClosedEyeH) {
        c.fillRoundRect(cx - w / 2, cy - 2, w, 5, 2, 1);  // closed eye
        return;
    }
    if (mood_ == Mood::Love) {
        const int16_t s = (int16_t)(w * 0.5f * min(1.0f, open));
        c.fillCircle(cx - s / 2, cy - s / 4, s / 2 + 1, 1);
        c.fillCircle(cx + s / 2, cy - s / 4, s / 2 + 1, 1);
        c.fillTriangle(cx - s - 1, cy - s / 8, cx + s + 1, cy - s / 8, cx, cy + s, 1);
        return;
    }
    const int16_t top = cy - h / 2;
    c.fillRoundRect(cx - w / 2, top, w, h, min<int16_t>(kEyeR, h / 2), 1);
    const int16_t outerX = cx + side * (w / 2 + 2), innerX = cx - side * (w / 2 + 2);
    const int16_t lid = top + h / 2;
    switch (mood_) {
    case Mood::Happy:
        c.fillCircle(cx, top + h + 6, (int16_t)(w * 0.6f), 0);  // leaves an upside-down "U"
        break;
    case Mood::Wink:
        if (side < 0) {
            c.fillCircle(cx, top + h + 6, (int16_t)(w * 0.6f), 0);
        }
        break;
    case Mood::Sad:
        c.fillTriangle(innerX, top - 1, outerX, top - 1, outerX, lid, 0);
        break;
    case Mood::Angry:
        c.fillTriangle(outerX, top - 1, innerX, top - 1, innerX, lid, 0);
        break;
    case Mood::Sleepy:
        c.fillRect(cx - w / 2 - 1, top - 1, w + 2, (int16_t)(h * 0.45f), 0);
        break;
    default:
        break;
    }
}

void FaceUi::drawMouth(int16_t mx, int16_t my)
{
    GFXcanvas1 &c = *canvas_;
    if (drawnPose_.mouth > 0.05f) {
        const int16_t h = 4 + (int16_t)(drawnPose_.mouth * 24);
        const int16_t w = 30 - (int16_t)(drawnPose_.mouth * 6);
        c.fillRoundRect(mx - w / 2, my - h / 2, w, h, min<int16_t>(h / 2, 10), 1);
        return;
    }
    if (asleep_) {
        c.fillRoundRect(mx - 8, my - 1, 16, 4, 2, 1);
        return;
    }
    switch (mood_) {
    case Mood::Happy:
    case Mood::Love:
    case Mood::Wink:
        c.fillCircle(mx, my - 14, 24, 1);
        c.fillCircle(mx, my - 22, 26, 0);
        break;
    case Mood::Sad:
        c.fillCircle(mx, my + 18, 22, 1);
        c.fillCircle(mx, my + 26, 24, 0);
        break;
    case Mood::Surprised:
        c.fillCircle(mx, my, 11, 1);
        c.fillCircle(mx, my, 6, 0);
        break;
    case Mood::Angry:
        c.fillRoundRect(mx - 16, my - 2, 32, 5, 2, 1);
        break;
    case Mood::Thinking:
        c.fillRoundRect(mx + 4, my - 2, 18, 5, 2, 1);
        break;
    case Mood::Sleepy:
        c.fillCircle(mx, my, 5, 1);
        c.fillCircle(mx, my, 2, 0);
        break;
    default:
        c.fillRoundRect(mx - 14, my - 3, 28, 6, 3, 1);
        break;
    }
}

void FaceUi::drawExtras()
{
    GFXcanvas1 &c = *canvas_;
    if (asleep_) {
        c.setTextColor(1);
        for (uint8_t i = 0; i <= drawnPhase_ % 3; ++i) {
            c.setTextSize(1 + i);
            c.setCursor(262 + i * 16, 40 - i * 16);
            c.print('z');
        }
    } else if (thinking_) {
        for (uint8_t i = 0; i <= drawnPhase_ % 3; ++i) {
            c.fillCircle(272 + i * 12, 22 - i * 8, 2 + i * 2, 1);
        }
    }
}

void FaceUi::blit(uint16_t color)
{
    const uint8_t *buf = canvas_->getBuffer();
    const int16_t stride = (kFaceW + 7) / 8;
    for (int16_t y0 = 0; y0 < kFaceH; y0 += kChunkRows) {
        const int16_t rows = min<int16_t>(kChunkRows, kFaceH - y0);
        uint16_t *out = lineBuf_;
        for (int16_t y = y0; y < y0 + rows; ++y) {
            const uint8_t *row = buf + y * stride;
            for (int16_t x = 0; x < kFaceW; ++x) {
                *out++ = (row[x >> 3] & (0x80 >> (x & 7))) ? color : kColorBlack;
            }
        }
        board_hw::pushImage(0, kFaceTop + y0, kFaceW, rows, lineBuf_);
    }
}

void FaceUi::render()
{
    canvas_->fillScreen(0);
    const int16_t lx = (int16_t)(drawnPose_.lookX * kLookX), ly = (int16_t)(drawnPose_.lookY * kLookY);
    drawEye(kFaceW / 2 - kEyeDX + lx, kEyeY + ly, drawnPose_.openL, -1);
    drawEye(kFaceW / 2 + kEyeDX + lx, kEyeY + ly, drawnPose_.openR, 1);
    drawMouth(kFaceW / 2 + lx / 2, kMouthY + ly / 2);
    drawExtras();
    blit(moodColor());
}

// ---------------------------------------------------------------- input and loop

FaceAction FaceUi::press(int16_t x, int16_t y)
{
    lastInteractionMs_ = millis();
    if (backTapped(x, y)) {
        return FaceAction::Back;
    }
    if (kComposeButton.contains(x, y)) {
        if (asleep_) {
            asleep_ = false;
            setMood(Mood::Neutral, 0);
        }
        return FaceAction::Compose;
    }
    if (y >= kSubTop) {
        if (speaking_) {
            pageStartMs_ = 0;  // show the rest of the page now; the hold then moves on
        }
        return FaceAction::None;
    }
    if (y >= kFaceTop) {
        react(x, y);
    }
    return FaceAction::None;
}

void FaceUi::loop(uint32_t nowMs)
{
    updateSpeech(nowMs);
    updateIdle(nowMs);
    if (!visible_ || canvas_ == nullptr || nowMs - lastFrameMs_ < kFrameMs) {
        return;
    }
    lastFrameMs_ = nowMs;

    if (thinking_) {
        // Animated dots after "Claude dang nghi".
        static const char *const kDots[] = {"   ", ".  ", ".. ", "..."};
        gfx_.setTextSize(2);
        gfx_.setTextColor(kColorSystem, kColorBlack);
        gfx_.setCursor(kSubX + 16 * kFontW * 2, kSubTop + 30);
        gfx_.print(kDots[(nowMs / kPhaseMs) % 4]);
    }

    const Pose target = targetPose(nowMs);
    pose_.lookX += (target.lookX - pose_.lookX) * kFollow;
    pose_.lookY += (target.lookY - pose_.lookY) * kFollow;
    pose_.openL += (target.openL - pose_.openL) * kFollow;
    pose_.openR += (target.openR - pose_.openR) * kFollow;
    pose_.mouth += (target.mouth - pose_.mouth) * kMouthFollow;

    // Blinks are fast and not smoothed; smiling "^ ^" eyes do not blink.
    Pose frame = pose_;
    const uint32_t sinceBlink = nowMs - blinkStartMs_;
    const bool canBlink = !asleep_ && mood_ != Mood::Happy && mood_ != Mood::Wink;
    if (canBlink && sinceBlink < 2 * kBlinkMs) {
        const float f = fabsf(1.0f - (float)sinceBlink / kBlinkMs);
        frame.openL *= max(0.05f, f);
        frame.openR *= max(0.05f, f);
    }
    const uint8_t phase = (asleep_ || thinking_) ? (uint8_t)((nowMs / kPhaseMs) % 3) : 0;
    if (!forceRender_ && !(frame != drawnPose_) && mood_ == drawnMood_ && phase == drawnPhase_) {
        return;
    }
    forceRender_ = false;
    drawnPose_ = frame;
    drawnMood_ = mood_;
    drawnPhase_ = phase;
    render();
}
