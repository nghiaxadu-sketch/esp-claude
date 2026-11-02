// Radio and music player for the CYD speaker: MP3 from an HTTP (Shoutcast/Icecast) stream or from a file on
// the microSD card, decoded by the "ESP8266Audio" library and played through the ESP32's internal DAC on
// GPIO26 (the "SPEAK" connector amplifier). Decoding runs in its own FreeRTOS task so the screen stays
// responsive. HTTPS streams are not supported: the TLS session would not leave enough RAM for the decoder.
#pragma once

#include <Arduino.h>

namespace audio_player {

constexpr size_t kLevelBars = 16;  // sound visualizer columns

enum class Source : uint8_t { None, Radio, File };

struct Status {
    Source source = Source::None;
    bool running = false;    // decoder task alive (connecting, buffering or playing)
    bool playing = false;    // samples are reaching the speaker
    String name;             // station or file name (ASCII)
    String title;            // stream title from ICY metadata ("Artist - Song"), ASCII
    String error;            // last error, ASCII, "" = none
    uint16_t kbps = 0;       // stream bitrate from the ICY headers, 0 = unknown
    uint32_t sampleRate = 0;
    uint32_t positionSec = 0;  // seconds played
};

// True on boards whose speaker pin is a DAC output.
bool supported();

// Starts playing; stops whatever played before. False (with Status::error set) when it cannot start.
bool playUrl(const String &url, const String &name);
bool playFile(const String &path, const String &name);
void stop();
bool active();

Status status();
// Recent loudness per visualizer column, 0..255 (newest last).
void levels(uint8_t (&out)[kLevelBars]);

void setVolume(uint8_t percent);
uint8_t volume();

}  // namespace audio_player
