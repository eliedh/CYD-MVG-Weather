#include "hardware.h"

#include "../app/log.h"
#include "board_config.h"

namespace hw {

LGFX_CYD lcd;

static uint8_t s_blTarget = 0, s_blLevel = 0;

void begin() {
  // RGB LED is active LOW and too dim to be useful: switch it off.
  for (int pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

  // LDR quirk: one analogRead() BEFORE setting the attenuation, otherwise the
  // ADC reports an error and keeps reading 0.
  analogRead(PIN_LDR);
  analogSetPinAttenuation(PIN_LDR, ADC_0db);

  lcd.init();
  lcd.setRotation(PANEL_ROTATION);
  lcd.setBrightness(0);
  lcd.fillScreen(TFT_BLACK);
}

Touch pollTouch() {
  static bool wasDown = false;
  static uint32_t downAt = 0;
  static int startX = 0, startY = 0, lastX = 0, lastY = 0;
  static bool longFired = false;
  static uint32_t lastSample = 0;

  Touch t;
  uint32_t now = millis();
  if (now - lastSample < 15) {  // ~60 Hz sampling is plenty
    t.down = wasDown;
    return t;
  }
  lastSample = now;

  lgfx::touch_point_t tp;
  bool down = lcd.getTouch(&tp, 1) > 0;
  if (down) {
    lastX = tp.x;
    lastY = tp.y;
    if (!wasDown) {
      downAt = now;
      startX = tp.x;
      startY = tp.y;
      longFired = false;
    } else if (!longFired && now - downAt >= 1000) {
      longFired = true;
      t.event = TouchEvent::LongPress;
      t.x = startX;
      t.y = startY;
    }
  } else if (wasDown) {
    uint32_t held = now - downAt;
    // A tap must be short and roughly stationary. Movement tolerance is
    // generous because readings on this panel are noisy.
    int dx = abs(lastX - startX), dy = abs(lastY - startY);
    if (!longFired && held >= 30 && held < 900 && dx < 40 && dy < 40) {
      t.event = TouchEvent::Tap;
      t.x = startX;
      t.y = startY;
    }
  }
  wasDown = down;
  t.down = down;
  return t;
}

static bool s_bootArmed = false;
static bool s_bootWasDown = false;

void bootDisarm() {
  s_bootArmed = false;
  s_bootWasDown = false;
}

uint32_t bootHeldMs(uint32_t* releasedAfterMs) {
  static uint32_t downAt = 0;
  static uint32_t highSince = 0;
  static bool warned = false;
  bool& wasDown = s_bootWasDown;
  bool down = digitalRead(PIN_BOOT_BUTTON) == LOW;
  uint32_t now = millis();
  if (releasedAfterMs) *releasedAfterMs = 0;

  // GPIO0 is also wired to the USB-serial auto-reset circuit: some serial
  // monitors hold it LOW while the port is open. The button only counts after
  // it has been seen released for 300 ms, so a pin that is low from power-on
  // (or stuck) can never trigger anything.
  if (!s_bootArmed) {
    if (down) {
      highSince = 0;
      if (!warned) {
        warned = true;
        LOGW("BOOT/GPIO0 reads LOW - ignored until released (serial monitor DTR/RTS?)");
      }
      return 0;
    }
    if (!highSince) highSince = now;
    if (now - highSince < 300) return 0;
    s_bootArmed = true;
    wasDown = false;
    warned = false;
    LOGI("BOOT button armed");
  }

  if (down && !wasDown) {
    downAt = now;
    LOGI("BOOT button pressed");
  }
  if (!down && wasDown && releasedAfterMs) *releasedAfterMs = now - downAt;
  wasDown = down;
  return down ? now - downAt : 0;
}

int readLdrRaw() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(PIN_LDR);
  return (int)(sum / 8);
}

void setBacklightTarget(uint8_t level) { s_blTarget = level; }

void updateBacklight() {
  if (s_blLevel == s_blTarget) return;
  // Gentle fade: ~8 steps per call, called every ~20 ms.
  int diff = (int)s_blTarget - (int)s_blLevel;
  int step = diff > 0 ? (diff > 8 ? 8 : diff) : (diff < -8 ? -8 : diff);
  s_blLevel = (uint8_t)(s_blLevel + step);
  lcd.setBrightness(s_blLevel);
}

uint8_t backlightLevel() { return s_blLevel; }

}  // namespace hw
