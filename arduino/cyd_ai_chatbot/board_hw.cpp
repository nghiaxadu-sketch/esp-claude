#include "board_hw.h"

#include <Adafruit_ILI9341.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <algorithm>

#include "board_pins.h"
#include "config.h"
#include "settings_store.h"
#include "ui_common.h"

namespace board_hw {
namespace {

constexpr int16_t kCalMargin = 30;
constexpr int16_t kCalMinRawSpan = 400;
constexpr uint32_t kCalStableMs = 300;
constexpr int16_t kCrossHalf = 10;
constexpr int16_t kCrossCircle = 6;
constexpr int kLightRawMax = 4095;
constexpr uint8_t kMinBrightnessSetting = 5;

#if CYD_SHARED_SPI
SPIClass &lcdSpi = SPI;
SPIClass &touchSpi = SPI;
#else
SPIClass lcdSpiBus(HSPI);  // ESP32-2432S028R: LCD on HSPI, touch on the default (VSPI) bus
SPIClass &lcdSpi = lcdSpiBus;
SPIClass &touchSpi = SPI;
#endif

#if CYD_SHARED_SPI || CYD_DISPLAY_ST7789
constexpr bool kUseSt7789 = true;
Adafruit_ST7789 tft(&lcdSpi, CYD_PIN_LCD_CS, CYD_PIN_LCD_DC, CYD_PIN_LCD_RST);
#else
constexpr bool kUseSt7789 = false;
Adafruit_ILI9341 tft(&lcdSpi, CYD_PIN_LCD_DC, CYD_PIN_LCD_CS, CYD_PIN_LCD_RST);
#endif
// The "2USB" ESP32-2432S028R ST7789 panels need inversion; ILI9341 and NM-CYD-C5 do not.
constexpr bool kDefaultInvert = kUseSt7789 && !CYD_SHARED_SPI;

TouchXpt2046 touchPanel(touchSpi, CYD_PIN_TOUCH_CS, ui::kScreenW, ui::kScreenH);
uint8_t backlightPercent = LCD_BRIGHTNESS_PERCENT;
uint8_t brightnessPercent = LCD_BRIGHTNESS_PERCENT;
bool screenFlipped = false;
bool soundOn = true;
bool speakerReady = false;
uint32_t beepStopMs = 0;
bool beeping = false;

void initLed()
{
#if RGB_LED_ENABLED && !CYD_RGB_WS2812
    for (int8_t pin : {CYD_PIN_LED_RED, CYD_PIN_LED_GREEN, CYD_PIN_LED_BLUE}) {
        ledcAttach(pin, RGB_PWM_HZ, RGB_PWM_BITS);
    }
#endif
    setLed(0, 0, 0);
}

// Waits for a stable press and its release; returns the averaged raw reading.
TouchRaw waitForCalibrationTap()
{
    TouchRaw raw = {};
    while (true) {
        TouchRaw sample;
        if (touchPanel.readRaw(sample)) {
            int32_t sx = 0, sy = 0, n = 0;
            const uint32_t start = millis();
            while (millis() - start < kCalStableMs) {
                if (touchPanel.readRaw(sample)) {
                    sx += sample.x;
                    sy += sample.y;
                    ++n;
                }
                delay(5);
            }
            if (n > 0) {
                raw.x = sx / n;
                raw.y = sy / n;
                while (touchPanel.readRaw(sample)) {  // wait for release
                    delay(10);
                }
                delay(150);
                return raw;
            }
        }
        delay(10);
    }
}

void drawCrosshair(int16_t x, int16_t y, uint16_t color)
{
    tft.drawFastHLine(x - kCrossHalf, y, 2 * kCrossHalf + 1, color);
    tft.drawFastVLine(x, y - kCrossHalf, 2 * kCrossHalf + 1, color);
    tft.drawCircle(x, y, kCrossCircle, color);
}

// The calibration is stored for the unflipped screen; readTouch() turns it around when flipped.
int16_t unflipX(int16_t x)
{
    return screenFlipped ? ui::kScreenW - 1 - x : x;
}

int16_t unflipY(int16_t y)
{
    return screenFlipped ? ui::kScreenH - 1 - y : y;
}

TouchRaw calibrationPoint(int16_t x, int16_t y)
{
    ui::drawSplash(tft, "Hieu chinh cam ung", "Cham va giu vao tam dau +", "(dung que nhua hoac mong tay)");
    drawCrosshair(x, y, ui::kColorWhite);
    const TouchRaw raw = waitForCalibrationTap();
    Serial.printf("[touch] screen(%d,%d) -> raw(%d,%d)\n", x, y, raw.x, raw.y);
    return raw;
}

}  // namespace

void begin()
{
    initLed();
    // Deselect every SPI device before the first transfer.
    for (int8_t cs : {CYD_PIN_LCD_CS, CYD_PIN_TOUCH_CS, CYD_PIN_SD_CS}) {
        pinMode(cs, OUTPUT);
        digitalWrite(cs, HIGH);
    }
    lcdSpi.begin(CYD_PIN_LCD_SCK, CYD_PIN_LCD_MISO, CYD_PIN_LCD_MOSI);
#if !CYD_SHARED_SPI
    touchSpi.begin(CYD_PIN_TOUCH_SCK, CYD_PIN_TOUCH_MISO, CYD_PIN_TOUCH_MOSI);
#endif

    ledcAttach(CYD_PIN_LCD_BACKLIGHT, BACKLIGHT_PWM_HZ, BACKLIGHT_PWM_BITS);
    setBacklight(LCD_BRIGHTNESS_PERCENT);

#if CYD_SHARED_SPI || CYD_DISPLAY_ST7789
    tft.init(LCD_NATIVE_WIDTH, LCD_NATIVE_HEIGHT);
    tft.setSPISpeed(LCD_SPI_HZ);
#else
    tft.begin(LCD_SPI_HZ);
#endif
    tft.setRotation(LCD_ROTATION);
    tft.invertDisplay(LCD_INVERT_COLORS < 0 ? kDefaultInvert : LCD_INVERT_COLORS != 0);
    tft.setTextWrap(false);
    touchPanel.begin();

    if (CYD_PIN_LIGHT_SENSOR >= 0) {
        pinMode(CYD_PIN_LIGHT_SENSOR, INPUT);
    }
    pinMode(CYD_PIN_BOOT_BUTTON, INPUT_PULLUP);
}

Adafruit_GFX &display()
{
    return tft;
}

TouchXpt2046 &touch()
{
    return touchPanel;
}

bool readTouch(int16_t &x, int16_t &y)
{
    if (!touchPanel.read(x, y)) {
        return false;
    }
    x = unflipX(x);
    y = unflipY(y);
    return true;
}

void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t *pixels)
{
    tft.drawRGBBitmap(x, y, pixels, w, h);  // Adafruit_SPITFT version: one address window, burst write
}

void setFlipped(bool flipped)
{
    screenFlipped = flipped;
    tft.setRotation(flipped ? (LCD_ROTATION + 2) % 4 : LCD_ROTATION);
}

bool flipped()
{
    return screenFlipped;
}

void setLed(uint8_t r, uint8_t g, uint8_t b)
{
#if RGB_LED_ENABLED
    const uint8_t rl = (uint16_t)r * RGB_LED_LEVEL / 255;
    const uint8_t gl = (uint16_t)g * RGB_LED_LEVEL / 255;
    const uint8_t bl = (uint16_t)b * RGB_LED_LEVEL / 255;
#if CYD_RGB_WS2812
    rgbLedWrite(CYD_PIN_RGB_LED, rl, gl, bl);
#else
    const uint32_t maxDuty = (1U << RGB_PWM_BITS) - 1;  // LEDs are active low
    ledcWrite(CYD_PIN_LED_RED, maxDuty - rl);
    ledcWrite(CYD_PIN_LED_GREEN, maxDuty - gl);
    ledcWrite(CYD_PIN_LED_BLUE, maxDuty - bl);
#endif
#else
    (void)r;
    (void)g;
    (void)b;
#endif
}

void setBacklight(uint8_t percent)
{
    backlightPercent = min<uint8_t>(percent, 100);
    const uint32_t maxDuty = (1U << BACKLIGHT_PWM_BITS) - 1;
    ledcWrite(CYD_PIN_LCD_BACKLIGHT, maxDuty * backlightPercent / 100);
}

uint8_t backlight()
{
    return backlightPercent;
}

void setBrightnessSetting(uint8_t percent)
{
    brightnessPercent = (uint8_t)std::min<int>(std::max<int>(percent, kMinBrightnessSetting), 100);
    setBacklight(brightnessPercent);
}

uint8_t brightnessSetting()
{
    return brightnessPercent;
}

void setSoundEnabled(bool enabled)
{
    soundOn = enabled;
    if (!enabled) {
        stopBeep();
    }
}

bool soundEnabled()
{
    return soundOn;
}

bool hasSpeaker()
{
    return CYD_PIN_SPEAKER >= 0 && SPEAKER_ENABLED;
}

void beep(uint32_t freqHz, uint32_t durationMs)
{
    if (!hasSpeaker() || !soundOn || freqHz == 0 || durationMs == 0) {
        return;
    }
    if (!speakerReady) {
        speakerReady = ledcAttach(CYD_PIN_SPEAKER, freqHz, SPEAKER_PWM_BITS);
        if (!speakerReady) {
            return;
        }
    }
    ledcWriteTone(CYD_PIN_SPEAKER, freqHz);
    beepStopMs = millis() + durationMs;
    beeping = true;
}

int lightPercent()
{
    if (CYD_PIN_LIGHT_SENSOR < 0) {
        return -1;
    }
    // The LDR pulls the pin low in bright light.
    const int raw = analogRead(CYD_PIN_LIGHT_SENSOR);
    return constrain(100 - raw * 100 / kLightRawMax, 0, 100);
}

bool bootButtonDown()
{
    return digitalRead(CYD_PIN_BOOT_BUTTON) == LOW;
}

bool bootButtonHeldAtStart(uint32_t windowMs)
{
    const uint32_t start = millis();
    while (millis() - start < windowMs) {
        if (bootButtonDown()) {
            return true;
        }
        delay(10);
    }
    return false;
}

void runTouchCalibration()
{
    while (true) {
        const int16_t left = kCalMargin, right = ui::kScreenW - kCalMargin;
        const int16_t top = kCalMargin, bottom = ui::kScreenH - kCalMargin;
        const TouchRaw p0 = calibrationPoint(left, top);
        const TouchRaw p1 = calibrationPoint(right, top);
        const TouchRaw p2 = calibrationPoint(left, bottom);

        // Moving along screen X should change one raw axis much more than the other.
        const bool swap = abs(p1.y - p0.y) > abs(p1.x - p0.x);
        TouchCalibration cal;
        cal.magic = TouchCalibration::kMagic;
        cal.swapXy = swap;
        cal.rawX0 = swap ? p0.y : p0.x;
        cal.rawX1 = swap ? p1.y : p1.x;
        cal.rawY0 = swap ? p0.x : p0.y;
        cal.rawY1 = swap ? p2.x : p2.y;
        cal.screenX0 = unflipX(left);
        cal.screenX1 = unflipX(right);
        cal.screenY0 = unflipY(top);
        cal.screenY1 = unflipY(bottom);

        if (abs(cal.rawX1 - cal.rawX0) >= kCalMinRawSpan && abs(cal.rawY1 - cal.rawY0) >= kCalMinRawSpan) {
            touchPanel.setCalibration(cal);
            settings_store::saveTouchCalibration(cal);
            ui::drawSplash(tft, "Xong!", "Da luu hieu chinh cam ung.");
            delay(800);
            return;
        }
        ui::drawSplash(tft, "Chua dat", "Cac diem cham qua gan nhau, lam lai nhe.");
        delay(1500);
    }
}

void stopBeep()
{
    if (beeping) {
        ledcWriteTone(CYD_PIN_SPEAKER, 0);
        beeping = false;
    }
}

void service()
{
    if (beeping && (int32_t)(millis() - beepStopMs) >= 0) {
        stopBeep();
    }
}

}  // namespace board_hw
