// Board hardware: display, touch, RGB LED, backlight, speaker, light sensor and BOOT button.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include "touch_xpt2046.h"

namespace board_hw {

void begin();
Adafruit_GFX &display();
TouchXpt2046 &touch();

void setLed(uint8_t r, uint8_t g, uint8_t b);
void setBacklight(uint8_t percent);
uint8_t backlight();

// Non-blocking tone on boards with a speaker; call service() from the loop to stop it on time.
bool hasSpeaker();
void beep(uint32_t freqHz, uint32_t durationMs);
void stopBeep();
// Ambient light 0..100, or -1 when the board has no light sensor.
int lightPercent();

bool bootButtonDown();
// True when BOOT is held during the first moments after power-up.
bool bootButtonHeldAtStart(uint32_t windowMs);

// Interactive 3-point calibration; saves the result to NVS.
void runTouchCalibration();

void service();

}  // namespace board_hw
