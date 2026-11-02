// "Radio" (online MP3 stations) and "Nhac SD" (MP3 files on the microSD card): a list page and a
// "now playing" page with the stream title, bitrate, play time, a sound visualizer and volume buttons.
// Playback keeps going on the home screen until "Dung" is pressed or another source starts.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <vector>

#include "radio_stations.h"
#include "ui_common.h"

enum class MediaMode : uint8_t { Radio, Music };
enum class MediaAction : uint8_t { None, Exit, VolumeChanged };

class MediaUi {
public:
    explicit MediaUi(Adafruit_GFX &gfx);

    void show(MediaMode mode);
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }
    MediaMode mode() const { return mode_; }

    MediaAction press(int16_t x, int16_t y);
    // "<" or a back swipe: playing page -> list -> home.
    MediaAction back();
    void loop(uint32_t nowMs);
    uint8_t volume() const { return volume_; }
    void setVolume(uint8_t percent) { volume_ = percent; }

private:
    enum class Page : uint8_t { List, Playing };
    struct Item {
        String name;      // ASCII for the screen
        String location;  // URL or SD path
    };
    static constexpr size_t kRows = 6;

    void loadItems();
    void draw();
    void drawList();
    void drawPlaying();
    void drawPlayingStatus();
    void drawVisualizer();
    void drawVolume();
    void play(size_t index);
    void step(int delta);

    Adafruit_GFX &gfx_;
    MediaMode mode_ = MediaMode::Radio;
    Page page_ = Page::List;
    bool visible_ = false;
    std::vector<Item> items_;
    String listNote_;
    size_t top_ = 0;
    size_t current_ = 0;
    uint8_t volume_ = 60;
    uint32_t lastStatusMs_ = 0;
    uint32_t lastVizMs_ = 0;
    String shownStatus_;
    String shownTitle_;
};
