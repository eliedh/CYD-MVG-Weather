// Board I/O besides the display: RGB LED (off), LDR, BOOT button, touch
// gesture detection (tap / long-press only) and smoothed backlight.
#pragma once

#include <Arduino.h>

#include "lgfx_cyd.h"

namespace hw {

extern LGFX_CYD lcd;

void begin();

// ---- touch: taps and long-press only (swipes are unreliable on this panel)
enum class TouchEvent : uint8_t { None, Tap, LongPress };
struct Touch {
  TouchEvent event = TouchEvent::None;
  int x = 0, y = 0;
  bool down = false;  // finger currently on the screen
};
Touch pollTouch();

// ---- BOOT button: returns how long it has been held (ms), 0 if released.
// `releasedAfterMs` is set once when the button is released.
// The button is ignored until it has been seen released after boot.
uint32_t bootHeldMs(uint32_t* releasedAfterMs);
// Ignore the button until it is released again (used when it looks stuck).
void bootDisarm();

// ---- LDR: 0 (bright) .. 4095 (dark). Averaged.
int readLdrRaw();

// ---- Backlight: set a target (0..255); stepped smoothly by update().
void setBacklightTarget(uint8_t level);
void updateBacklight();
uint8_t backlightLevel();

}  // namespace hw
