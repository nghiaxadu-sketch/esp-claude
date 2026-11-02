#include "audio_player.h"

#include <functional>

#include "board_hw.h"
#include "board_pins.h"
#include "text_utils.h"

#if CYD_HAS_DAC_SPEAKER
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutput.h>
#include <driver/dac_continuous.h>
#endif

namespace audio_player {
namespace {

String ascii(const char *utf8)
{
    return String(text_utils::utf8ToAscii(utf8 != nullptr ? utf8 : "").c_str());
}

#if CYD_HAS_DAC_SPEAKER

constexpr size_t kStreamBufferBytes = 12 * 1024;  // network jitter buffer in front of the decoder
constexpr size_t kDacChunk = 256;                 // 8-bit samples per DMA write
constexpr uint32_t kDacDescriptors = 6;
constexpr uint32_t kDefaultRate = 44100;
constexpr uint32_t kTaskStackBytes = 8192;
constexpr UBaseType_t kTaskPriority = 3;
constexpr BaseType_t kTaskCore = 1;
constexpr uint32_t kStopWaitMs = 4000;
constexpr uint32_t kStopPollMs = 20;
constexpr uint32_t kSamplesPerLevel = 1024;  // one visualizer column per ~23 ms at 44.1 kHz
constexpr uint32_t kBitrateWindowMs = 4000;
constexpr int16_t kDacMid = 128;

SemaphoreHandle_t lock = nullptr;
Status state;
uint8_t levelRing[kLevelBars];
size_t levelHead = 0;
volatile bool stopRequested = false;
volatile bool taskAlive = false;
volatile uint8_t volumePercent = 60;

struct Job {
    Source source;
    String location;  // URL or SD path
    String name;
};

void withLock(const std::function<void()> &fn)
{
    if (lock == nullptr) {
        lock = xSemaphoreCreateMutex();
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    fn();
    xSemaphoreGive(lock);
}

void setError(const String &text)
{
    withLock([&] { state.error = text; });
    Serial.printf("[audio] %s\n", text.c_str());
}

void pushLevel(uint8_t level)
{
    levelRing[levelHead] = level;
    levelHead = (levelHead + 1) % kLevelBars;
}

// Mono 8-bit output to DAC channel 2 (GPIO26) through the continuous (DMA) DAC driver.
class DacOutput : public AudioOutput {
public:
    ~DacOutput() override { close(); }

    bool begin() override { return open(hertz != 0 ? hertz : kDefaultRate); }

    bool SetRate(int hz) override
    {
        hertz = (uint16_t)hz;
        if (handle_ != nullptr && (uint32_t)hz != rate_) {
            return open((uint32_t)hz);
        }
        return true;
    }

    bool ConsumeSample(int16_t sample[2]) override
    {
        if (handle_ == nullptr) {
            return false;
        }
        int32_t mono = channels == 1 ? sample[0] : ((int32_t)sample[0] + sample[1]) / 2;
        mono = mono * volumePercent / 100;
        const int32_t mag = mono < 0 ? -mono : mono;
        if (mag > peak_) {
            peak_ = mag;
        }
        if (++count_ >= kSamplesPerLevel) {
            pushLevel((uint8_t)min<int32_t>(255, peak_ >> 6));
            peak_ = 0;
            count_ = 0;
            ++frames_;
        }
        chunk_[fill_++] = (uint8_t)((mono >> 8) + kDacMid);
        if (fill_ == kDacChunk) {
            flushChunk();
        }
        return true;
    }

    bool stop() override
    {
        flushChunk();
        close();
        return true;
    }

    uint32_t rate() const { return rate_; }
    uint64_t samplesPlayed() const { return played_; }

private:
    bool open(uint32_t hz)
    {
        close();
        dac_continuous_config_t cfg = {};
        cfg.chan_mask = DAC_CHANNEL_MASK_CH1;  // GPIO26
        cfg.desc_num = kDacDescriptors;
        cfg.buf_size = kDacChunk;
        cfg.freq_hz = hz;
        cfg.offset = 0;
        cfg.clk_src = DAC_DIGI_CLK_SRC_APLL;  // APLL reaches the low MP3 rates (8..22 kHz) too
        cfg.chan_mode = DAC_CHANNEL_MODE_SIMUL;
        if (dac_continuous_new_channels(&cfg, &handle_) != ESP_OK) {
            handle_ = nullptr;
            return false;
        }
        if (dac_continuous_enable(handle_) != ESP_OK) {
            dac_continuous_del_channels(handle_);
            handle_ = nullptr;
            return false;
        }
        rate_ = hz;
        return true;
    }

    void close()
    {
        if (handle_ != nullptr) {
            dac_continuous_disable(handle_);
            dac_continuous_del_channels(handle_);
            handle_ = nullptr;
        }
    }

    void flushChunk()
    {
        if (handle_ != nullptr && fill_ > 0) {
            size_t loaded = 0;
            dac_continuous_write(handle_, chunk_, fill_, &loaded, -1);
            played_ += fill_;
        }
        fill_ = 0;
    }

    dac_continuous_handle_t handle_ = nullptr;
    uint32_t rate_ = 0;
    uint8_t chunk_[kDacChunk];
    size_t fill_ = 0;
    int32_t peak_ = 0;
    uint32_t count_ = 0;
    uint32_t frames_ = 0;
    uint64_t played_ = 0;
};

// Pass-through source that counts bytes, for the "kbps" shown on the radio screen.
class CountingSource : public AudioFileSource {
public:
    explicit CountingSource(AudioFileSource *in) : in_(in) {}
    uint32_t read(void *data, uint32_t len) override { return count(in_->read(data, len)); }
    uint32_t readNonBlock(void *data, uint32_t len) override { return count(in_->readNonBlock(data, len)); }
    bool seek(int32_t pos, int dir) override { return in_->seek(pos, dir); }
    bool close() override { return in_->close(); }
    bool isOpen() override { return in_->isOpen(); }
    uint32_t getSize() override { return in_->getSize(); }
    uint32_t getPos() override { return in_->getPos(); }
    bool loop() override { return in_->loop(); }

    // Bitrate over the last few seconds, kbit/s.
    uint16_t kbps()
    {
        const uint32_t now = millis();
        if (now - windowStart_ >= kBitrateWindowMs) {
            lastKbps_ = (uint16_t)(windowBytes_ * 8 / (now - windowStart_));
            windowBytes_ = 0;
            windowStart_ = now;
        }
        return lastKbps_;
    }

private:
    uint32_t count(uint32_t n)
    {
        windowBytes_ += n;
        return n;
    }

    AudioFileSource *in_;
    uint32_t windowBytes_ = 0;
    uint32_t windowStart_ = millis();
    uint16_t lastKbps_ = 0;
};

void onMetadata(void *, const char *type, bool, const char *text)
{
    if (type != nullptr && strcmp(type, "StreamTitle") == 0 && text != nullptr) {
        const String title = ascii(text);
        withLock([&] { state.title = title; });
    }
}

void onStatus(void *, int code, const char *text)
{
    Serial.printf("[audio] status %d: %s\n", code, text != nullptr ? text : "");
}

void playerTask(void *arg)
{
    Job *job = static_cast<Job *>(arg);
    AudioFileSource *base = nullptr;
    CountingSource *counter = nullptr;
    AudioFileSourceBuffer *buffer = nullptr;
    AudioGeneratorMP3 *mp3 = nullptr;
    DacOutput *out = nullptr;
    AudioFileSource *input = nullptr;

    if (job->source == Source::Radio) {
        AudioFileSourceICYStream *icy = new AudioFileSourceICYStream(job->location.c_str());
        base = icy;
        if (!icy->isOpen()) {
            setError("Khong mo duoc dai (mang hoac URL sai)");
        } else {
            icy->RegisterMetadataCB(onMetadata, nullptr);
            counter = new CountingSource(icy);
            buffer = new AudioFileSourceBuffer(counter, kStreamBufferBytes);
            buffer->RegisterStatusCB(onStatus, nullptr);
            input = buffer;
        }
    } else {
        base = new AudioFileSourceSD(job->location.c_str());
        if (!base->isOpen()) {
            setError("Khong mo duoc file tren the SD");
        } else {
            input = base;
        }
    }

    if (input != nullptr) {
        out = new DacOutput();
        mp3 = new AudioGeneratorMP3();
        mp3->RegisterStatusCB(onStatus, nullptr);
        if (!mp3->begin(input, out)) {
            setError("Khong giai ma duoc MP3 (dinh dang khong ho tro?)");
        } else {
            withLock([&] { state.playing = true; });
            const uint32_t start = millis();
            while (!stopRequested && mp3->isRunning()) {
                if (!mp3->loop()) {
                    mp3->stop();
                    if (!stopRequested && job->source == Source::Radio) {
                        setError("Mat ket noi dai radio");
                    }
                    break;
                }
                const uint16_t kbps = counter != nullptr ? counter->kbps() : 0;
                const uint32_t rate = out->rate();
                const uint32_t pos = job->source == Source::File && rate > 0 ? (uint32_t)(out->samplesPlayed() / rate)
                                                                              : (millis() - start) / 1000;
                withLock([&] {
                    state.kbps = kbps;
                    state.sampleRate = rate;
                    state.positionSec = pos;
                });
            }
            if (mp3->isRunning()) {
                mp3->stop();
            }
        }
    }

    delete mp3;
    delete out;
    delete buffer;
    delete counter;
    delete base;
    delete job;
    withLock([&] {
        state.running = false;
        state.playing = false;
    });
    memset(levelRing, 0, sizeof(levelRing));
    taskAlive = false;
    vTaskDelete(nullptr);
}

bool start(Source source, const String &location, const String &name)
{
    stop();
    if (lock == nullptr) {
        lock = xSemaphoreCreateMutex();
    }
    board_hw::releaseSpeakerForDac();
    withLock([&] {
        state = Status();
        state.source = source;
        state.running = true;
        state.name = name;
    });
    Job *job = new Job{source, location, name};
    stopRequested = false;
    taskAlive = true;
    if (xTaskCreatePinnedToCore(playerTask, "audio", kTaskStackBytes, job, kTaskPriority, nullptr, kTaskCore) !=
        pdPASS) {
        taskAlive = false;
        delete job;
        withLock([&] { state.running = false; });
        setError("Thieu RAM de phat am thanh");
        board_hw::reclaimSpeaker();
        return false;
    }
    return true;
}

#endif  // CYD_HAS_DAC_SPEAKER

}  // namespace

bool supported()
{
    return CYD_HAS_DAC_SPEAKER && board_hw::hasSpeaker();
}

#if CYD_HAS_DAC_SPEAKER

bool playUrl(const String &url, const String &name)
{
    if (url.startsWith("https://")) {
        withLock([&] {
            state = Status();
            state.source = Source::Radio;
            state.name = name;
            state.error = "Dai nay can HTTPS - khong du bo nho";
        });
        return false;
    }
    return start(Source::Radio, url, name);
}

bool playFile(const String &path, const String &name)
{
    if (!board_hw::sdMount()) {
        withLock([&] { state.error = "Chua co the SD"; });
        return false;
    }
    return start(Source::File, path, name);
}

void stop()
{
    if (taskAlive) {
        stopRequested = true;
        const uint32_t startMs = millis();
        while (taskAlive && millis() - startMs < kStopWaitMs) {
            delay(kStopPollMs);
        }
    }
    board_hw::reclaimSpeaker();
}

bool active()
{
    return taskAlive;
}

Status status()
{
    Status copy;
    withLock([&] { copy = state; });
    return copy;
}

void levels(uint8_t (&out)[kLevelBars])
{
    for (size_t i = 0; i < kLevelBars; ++i) {
        out[i] = levelRing[(levelHead + i) % kLevelBars];
    }
}

void setVolume(uint8_t percent)
{
    volumePercent = min<uint8_t>(percent, 100);
}

uint8_t volume()
{
    return volumePercent;
}

#else  // no DAC speaker on this board

bool playUrl(const String &, const String &) { return false; }
bool playFile(const String &, const String &) { return false; }
void stop() {}
bool active() { return false; }
Status status()
{
    Status s;
    s.error = "Board nay khong co loa DAC";
    return s;
}
void levels(uint8_t (&out)[kLevelBars]) { memset(out, 0, sizeof(out)); }
void setVolume(uint8_t) {}
uint8_t volume() { return 0; }

#endif

}  // namespace audio_player
