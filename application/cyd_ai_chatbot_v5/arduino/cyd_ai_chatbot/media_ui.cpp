#include "media_ui.h"

#include <SD.h>

#include "audio_player.h"
#include "board_hw.h"

namespace {

using namespace ui;

constexpr int16_t kListTop = kHeaderH + 2;
constexpr int16_t kRowH = 30;
constexpr int16_t kTitleY = 34;
constexpr int16_t kSongY = 58;
constexpr int16_t kStatusY = 84;
constexpr int16_t kVizTop = 100;
constexpr int16_t kVizBottom = 176;
constexpr int16_t kVizGap = 3;
constexpr int16_t kVolumeY = 184;
constexpr uint32_t kStatusRefreshMs = 500;
constexpr uint32_t kVizRefreshMs = 60;
constexpr uint8_t kVolumeStep = 10;
constexpr size_t kMaxSdFiles = 100;
constexpr size_t kNameChars = 48;
constexpr const char *const kSdFolders[] = {"/", "/music", "/MUSIC", "/nhac"};

// Footer of the playing page: previous, play/stop, next, volume down, volume up.
constexpr size_t kPlayButtons = 5;

bool isMp3(const String &name)
{
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".mp3");
}

String baseName(const String &path)
{
    const int slash = path.lastIndexOf('/');
    String name = slash >= 0 ? path.substring(slash + 1) : path;
    if (isMp3(name)) {
        name = name.substring(0, name.length() - 4);
    }
    return name;
}

String timeText(uint32_t sec)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
    return String(buf);
}

}  // namespace

MediaUi::MediaUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
}

void MediaUi::show(MediaMode mode)
{
    if (mode != mode_) {
        top_ = 0;
        current_ = 0;
    }
    mode_ = mode;
    visible_ = true;
    loadItems();
    const audio_player::Status st = audio_player::status();
    const bool sameSource = st.running && (st.source == audio_player::Source::Radio) == (mode == MediaMode::Radio);
    page_ = sameSource ? Page::Playing : Page::List;
    draw();
}

void MediaUi::loadItems()
{
    items_.clear();
    listNote_ = "";
    if (!audio_player::supported()) {
        listNote_ = "Board nay khong co loa DAC";
        return;
    }
    if (mode_ == MediaMode::Radio) {
        for (const RadioStation &s : radio_stations::load()) {
            items_.push_back({ellipsize(toAscii(s.name), kNameChars), s.url});
        }
        if (items_.empty()) {
            listNote_ = "Chua co dai radio - them tren trang web cua may";
        }
        return;
    }
    if (!board_hw::sdMount()) {
        listNote_ = "Chua cam the SD (FAT32) hoac the loi";
        return;
    }
    for (const char *folder : kSdFolders) {
        File dir = SD.open(folder);
        if (!dir || !dir.isDirectory()) {
            continue;
        }
        for (File f = dir.openNextFile(); f && items_.size() < kMaxSdFiles; f = dir.openNextFile()) {
            const String name = f.name();
            if (!f.isDirectory() && isMp3(name) && name[0] != '.') {
                String path = String(folder);
                if (!path.endsWith("/")) {
                    path += "/";
                }
                path += name;
                items_.push_back({ellipsize(toAscii(baseName(name)), kNameChars), path});
            }
            f.close();
        }
        dir.close();
    }
    if (items_.empty()) {
        listNote_ = "Khong co nhac trong the SD (file .mp3 o thu muc goc hoac /music)";
    }
}

void MediaUi::draw()
{
    gfx_.fillScreen(kColorBg);
    if (page_ == Page::List) {
        drawList();
    } else {
        drawPlaying();
    }
}

void MediaUi::drawList()
{
    const char *title = mode_ == MediaMode::Radio ? "Radio" : "Nhac the SD";
    drawBackHeader(gfx_, title, items_.empty() ? String() : String((unsigned)items_.size()) + " bai");
    if (items_.empty()) {
        drawCentered(gfx_, ellipsize(listNote_, kScreenW / kFontW), 0, kScreenW, 110, 1, kColorSystem);
    }
    for (size_t row = 0; row < kRows && top_ + row < items_.size(); ++row) {
        const size_t i = top_ + row;
        const int16_t y = kListTop + (int16_t)row * kRowH;
        const bool playingThis = audio_player::active() && i == current_;
        gfx_.fillRoundRect(kMargin, y + 1, kScreenW - 2 * kMargin, kRowH - 2, 4, playingThis ? kColorAccent : kColorHeader);
        gfx_.setTextSize(1);
        gfx_.setTextColor(playingThis ? kColorWhite : kColorMuted);
        gfx_.setCursor(10, y + 11);
        gfx_.print(String((unsigned)(i + 1)) + ".");
        gfx_.setTextColor(playingThis ? kColorWhite : kColorText);
        gfx_.setCursor(36, y + 11);
        gfx_.print(ellipsize(items_[i].name, 44));
    }
    drawButton(gfx_, footerSlot(0, 3), "Len", kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(1, 3), "Xuong", kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(2, 3), "Dang phat", kColorAccent, kColorWhite);
}

void MediaUi::drawPlaying()
{
    const char *title = mode_ == MediaMode::Radio ? "Radio" : "Nhac the SD";
    drawBackHeader(gfx_, title);
    const audio_player::Status st = audio_player::status();
    const String name = st.name.length() > 0 ? st.name : String("(chua chon bai)");
    drawCentered(gfx_, ellipsize(name, kScreenW / (kFontW * 2)), 0, kScreenW, kTitleY, 2, kColorAccent);
    shownStatus_ = "";
    shownTitle_ = "";
    drawPlayingStatus();
    drawVisualizer();
    drawVolume();
    static const char *const kLabels[kPlayButtons] = {"|<", "Dung", ">|", "-", "+"};
    for (size_t i = 0; i < kPlayButtons; ++i) {
        const char *label = i == 1 && !audio_player::active() ? "Phat" : kLabels[i];
        drawButton(gfx_, footerSlot(i, kPlayButtons), label, i == 1 ? kColorAccent : kColorButton, kColorWhite);
    }
}

void MediaUi::drawPlayingStatus()
{
    const audio_player::Status st = audio_player::status();
    String line;
    if (st.error.length() > 0) {
        line = st.error;
    } else if (!st.running) {
        line = mode_ == MediaMode::Radio ? "Radio - da dung" : "The nho - da dung";
    } else if (!st.playing) {
        line = "Dang ket noi / dem du lieu...";
    } else {
        line = mode_ == MediaMode::Radio ? "DANG PHAT - RADIO - LIVE" : "DANG PHAT - THE NHO";
        if (st.kbps > 0) {
            line += "  " + String(st.kbps) + " kbps";
        }
        line += "  " + timeText(st.positionSec);
    }
    const String song = st.title.length() > 0 ? st.title : String();
    if (line == shownStatus_ && song == shownTitle_) {
        return;
    }
    shownStatus_ = line;
    shownTitle_ = song;
    gfx_.fillRect(0, kSongY, kScreenW, kVizTop - kSongY - 2, kColorBg);
    if (song.length() > 0) {
        drawCentered(gfx_, ellipsize(song, kScreenW / kFontW), 0, kScreenW, kSongY, 1, kColorText);
    }
    drawCentered(gfx_, ellipsize(line, kScreenW / kFontW), 0, kScreenW, kStatusY,
                 1, st.error.length() > 0 ? kColorDanger : kColorMuted);
}

void MediaUi::drawVisualizer()
{
    uint8_t levels[audio_player::kLevelBars];
    audio_player::levels(levels);
    const int16_t barW = (kScreenW - 2 * kMargin - (int16_t)(audio_player::kLevelBars - 1) * kVizGap) /
                         (int16_t)audio_player::kLevelBars;
    const int16_t maxH = kVizBottom - kVizTop;
    for (size_t i = 0; i < audio_player::kLevelBars; ++i) {
        const int16_t x = kMargin + (int16_t)i * (barW + kVizGap);
        const int16_t h = (int16_t)((int32_t)levels[i] * maxH / 255);
        gfx_.fillRect(x, kVizTop, barW, maxH - h, kColorBg);
        gfx_.fillRect(x, kVizBottom - h, barW, h, kColorAccent);
    }
}

void MediaUi::drawVolume()
{
    gfx_.fillRect(0, kVolumeY, kScreenW, kFontH + 2, kColorBg);
    drawCentered(gfx_, "Am luong " + String(volume_) + "%", 0, kScreenW, kVolumeY, 1, kColorMuted);
}

void MediaUi::play(size_t index)
{
    if (index >= items_.size()) {
        return;
    }
    current_ = index;
    const Item &item = items_[index];
    audio_player::setVolume(volume_);
    if (mode_ == MediaMode::Radio) {
        audio_player::playUrl(item.location, item.name);
    } else {
        audio_player::playFile(item.location, item.name);
    }
    page_ = Page::Playing;
    draw();
}

void MediaUi::step(int delta)
{
    if (items_.empty()) {
        return;
    }
    const int n = (int)items_.size();
    play((size_t)(((int)current_ + delta + n) % n));
}

MediaAction MediaUi::back()
{
    if (page_ == Page::Playing) {
        page_ = Page::List;
        draw();
        return MediaAction::None;
    }
    return MediaAction::Exit;
}

MediaAction MediaUi::press(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        return back();
    }
    if (page_ == Page::List) {
        for (size_t i = 0; i < 3; ++i) {
            if (!footerSlot(i, 3).contains(x, y)) {
                continue;
            }
            if (i == 0 && top_ > 0) {
                top_ = top_ >= kRows ? top_ - kRows : 0;
                draw();
            } else if (i == 1 && top_ + kRows < items_.size()) {
                top_ += kRows;
                draw();
            } else if (i == 2) {
                page_ = Page::Playing;
                draw();
            }
            return MediaAction::None;
        }
        if (y >= kListTop && y < kListTop + (int16_t)kRows * kRowH) {
            play(top_ + (size_t)((y - kListTop) / kRowH));
        }
        return MediaAction::None;
    }
    for (size_t i = 0; i < kPlayButtons; ++i) {
        if (!footerSlot(i, kPlayButtons).contains(x, y)) {
            continue;
        }
        switch (i) {
        case 0: step(-1); break;
        case 1:
            if (audio_player::active()) {
                audio_player::stop();
                draw();
            } else {
                play(current_);
            }
            break;
        case 2: step(1); break;
        case 3:
        case 4:
            volume_ = (uint8_t)constrain((int)volume_ + (i == 4 ? kVolumeStep : -(int)kVolumeStep), 0, 100);
            audio_player::setVolume(volume_);
            board_hw::setVolume(volume_);
            drawVolume();
            return MediaAction::VolumeChanged;
        }
        return MediaAction::None;
    }
    return MediaAction::None;
}

void MediaUi::loop(uint32_t nowMs)
{
    if (!visible_ || page_ != Page::Playing) {
        return;
    }
    if (nowMs - lastStatusMs_ >= kStatusRefreshMs) {
        lastStatusMs_ = nowMs;
        drawPlayingStatus();
    }
    if (nowMs - lastVizMs_ >= kVizRefreshMs) {
        lastVizMs_ = nowMs;
        drawVisualizer();
    }
}
