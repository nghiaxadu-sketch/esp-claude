// Board side of the app engine: draws on the TFT and implements the functions in device_functions.h.
#pragma once

#include <Adafruit_GFX.h>

#include "mini_app.h"

// Draws apps on the TFT and keeps the round "<" back badge in the top-left corner on top of them.
class GfxCanvas : public mini_app::Canvas {
public:
    explicit GfxCanvas(Adafruit_GFX &gfx) : gfx_(gfx) {}

    // Repaints the back badge when the app drew over it since the last call.
    void refreshOverlay();
    void invalidateOverlay() { overlayDirty_ = true; }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) override;
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) override;
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) override;
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) override;
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
    void drawText(int16_t x, int16_t y, const std::string &ascii, uint8_t size, uint16_t color) override;

private:
    void touched(int16_t x, int16_t y, int16_t w, int16_t h);

    Adafruit_GFX &gfx_;
    bool overlayDirty_ = true;
};

class BoardDevice : public mini_app::Device {
public:
    uint32_t millis() override;
    bool hasFunction(const std::string &name, size_t argCount) const override;
    bool callFunction(const std::string &name, const std::vector<mini_script::Value> &args,
                      mini_script::Value &result, std::string &error) override;

    // Turns LED/sound off and restores the backlight after an app exits.
    void reset();

private:
    mini_script::Value httpGet(const std::string &url, const std::string &path);
    // GETs `url` (retrying once); apps often read several values of one response in a row, so the last body
    // is reused for a few seconds instead of opening a new TLS connection for each value.
    bool fetchBody(const std::string &url, std::string &body);

    std::string cachedUrl_;
    std::string cachedBody_;
    uint32_t cachedAtMs_ = 0;
};
