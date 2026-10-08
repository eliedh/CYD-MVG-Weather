#include "app.h"

#include <Arduino.h>
#include <time.h>

#include "../hw/hardware.h"
#include "../net/net_task.h"
#include "band_renderer.h"
#include "board_config.h"
#include "i18n.h"
#include "log.h"
#include "screens.h"
#include "shared.h"
#include "storage.h"
#include "theme.h"
#include "vm_builder.h"

Shared g;

namespace app {

namespace {

using ui::Screen;

enum class Overlay : uint8_t { None, Weather, Message, SettingsQR, ResetConfirm };

constexpr uint32_t kDetailTimeoutMs = 30 * 1000;
constexpr uint32_t kQrTimeoutMs = 90 * 1000;
constexpr uint32_t kNightWakeMs = 30 * 1000;
constexpr uint32_t kResetHoldShowMs = 2000;
constexpr uint32_t kResetCalibrateMs = 3000;
constexpr uint32_t kResetFactoryMs = 10000;

ui::BandRenderer renderer;
ui::ViewModel vm;
core::Settings settings;   // UI copy
core::DataSnapshot data;   // UI copy
uint32_t settingsVersion = 0, dataVersion = 0;
NetState net = NetState::Init;
bool timeValid = false;

Overlay overlay = Overlay::None;
uint32_t overlayDeadline = 0;
uint32_t wakeUntil = 0;
uint32_t lastActivity = 0;
bool bootHold = false;

uint32_t lastSig = 0;
uint32_t lastRenderMs = 0;
uint32_t nextLdrLog = 0;
float ldrEma = -1;
uint32_t dirtySince = 0;

uint32_t sig(std::initializer_list<uint32_t> parts) {
  uint32_t h = 2166136261u;
  for (uint32_t p : parts) {
    h ^= p;
    h *= 16777619u;
  }
  return h;
}

// ------------------------------------------------------------------ shared state

void pullShared() {
  SharedLock l;
  if (g.settingsVersion != settingsVersion) {
    settings = g.settings;
    settingsVersion = g.settingsVersion;
  }
  if (g.data.version != dataVersion) {
    data = g.data;
    dataVersion = g.data.version;
  }
  if (g.net != net) LOGI("net state %d -> %d", (int)net, (int)g.net);
  net = g.net;
  timeValid = g.timeValid;
  vm.wifiDown = g.wifiDown;
  strlcpy(vm.apSsid, g.apSsid, sizeof(vm.apSsid));
  strlcpy(vm.homeSsid, g.homeSsid, sizeof(vm.homeSsid));
  strlcpy(vm.ip, net == NetState::Portal && !g.ip[0] ? "192.168.4.1" : g.ip, sizeof(vm.ip));
  if (g.settingsDirty && !dirtySince) dirtySince = millis();
}

void persistIfDirty() {
  if (!dirtySince || millis() - dirtySince < 1500) return;  // debounce bursts of edits
  core::Settings copy;
  {
    SharedLock l;
    copy = g.settings;
    g.settingsDirty = false;
  }
  dirtySince = 0;
  storage::saveSettings(copy);
}

Screen baseScreen() {
  switch (net) {
    case NetState::Portal: return Screen::SetupAP;
    case NetState::Connecting: return Screen::Connecting;
    case NetState::Offline: return Screen::NoWifi;
    case NetState::Online:
      if (!timeValid) return Screen::Connecting;
      return settings.stops.empty() ? Screen::NeedStops : Screen::Main;
    default: return Screen::Boot;
  }
}

Screen currentScreen() {
  Screen base = baseScreen();
  if (overlay == Overlay::ResetConfirm) return Screen::ResetConfirm;
  if (overlay == Overlay::SettingsQR && net == NetState::Online) return Screen::SettingsQR;
  if (base != Screen::Main) return base;
  if (overlay == Overlay::Weather) return Screen::WeatherDetail;
  if (overlay == Overlay::Message && vm.messageCount > 0) return Screen::MessageDetail;
  return base;
}

void openOverlay(Overlay o) {
  overlay = o;
  overlayDeadline = millis() + (o == Overlay::SettingsQR || o == Overlay::ResetConfirm
                                    ? kQrTimeoutMs
                                    : kDetailTimeoutMs);
}

// ------------------------------------------------------------------ rendering

void render(bool force = false) {
  renderer.render([](ui::Painter& p) { ui::drawScreen(p, vm); }, force);
  lastRenderMs = millis();
}

void showNotice(const char* title, const char* body, uint32_t ms) {
  vm.screen = Screen::Notice;
  vm.noticeTitle = title;
  vm.noticeBody = body;
  render();
  uint32_t until = millis() + ms;
  while ((int32_t)(millis() - until) < 0) {
    hw::updateBacklight();
    delay(20);
  }
  vm.noticeTitle = vm.noticeBody = nullptr;
}

// ------------------------------------------------------------------ calibration / reset

void runCalibration() {
  LOGI("touch calibration started");
  hw::setBacklightTarget(255);
  vm.screen = Screen::CalibrateIntro;
  render(true);
  for (int i = 0; i < 125; i++) {  // 2.5 s, fading the backlight in
    hw::updateBacklight();
    delay(20);
  }
  renderer.finish();
  uint16_t cal[8];
  hw::lcd.calibrateTouch(cal, ui::theme::kAccent, ui::theme::kBg, 18);
  hw::lcd.setTouchCalibrate(cal);
  {
    SharedLock l;
    memcpy(g.settings.touchCal, cal, sizeof(cal));
    g.settings.touchCalValid = true;
    g.settingsVersion++;
    g.settingsDirty = true;
  }
  LOGI("touch calibration: %u %u %u %u %u %u %u %u", cal[0], cal[1], cal[2], cal[3], cal[4],
       cal[5], cal[6], cal[7]);
  renderer.invalidate();
  showNotice(core::tr(core::Str::CalDone, settings.lang), nullptr, 1200);
  lastSig = 0;
}

[[noreturn]] void factoryReset() {
  LOGI("factory reset");
  {
    SharedLock l;
    g.shuttingDown = true;  // net task stops touching Wi-Fi/NVS
  }
  hw::setBacklightTarget(200);
  showNotice(core::tr(core::Str::ResetDone, settings.lang), nullptr, 1500);
  storage::factoryReset();
  delay(300);
  ESP.restart();
  for (;;) {}
}

void handleBootButton() {
  uint32_t released = 0;
  uint32_t held = hw::bootHeldMs(&released);
  if (held >= kResetHoldShowMs) {
    bootHold = true;
    lastActivity = millis();
    vm.resetSeconds = held >= kResetFactoryMs ? 0 : (int)((kResetFactoryMs - held + 999) / 1000);
    vm.resetReleaseToCalibrate = held >= kResetCalibrateMs;
    if (held >= kResetFactoryMs) factoryReset();
  }
  if (released) {
    bootHold = false;
    if (released >= kResetCalibrateMs && released < kResetFactoryMs) runCalibration();
  }
}

// ------------------------------------------------------------------ backlight

bool isNightNow() {
  if (!timeValid) return false;
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);
  return settings.isNight(lt.tm_hour * 60 + lt.tm_min);
}

void updateBrightness(Screen screen) {
  uint32_t now = millis();
  int raw = hw::readLdrRaw();
  ldrEma = ldrEma < 0 ? raw : ldrEma * 0.9f + raw * 0.1f;

  int level = settings.brightness * 255 / 100;
  if (settings.autoBrightness) {
    float f;
    if (ldrEma <= LDR_BRIGHT_RAW) f = 1.0f;
    else if (ldrEma >= LDR_DARK_RAW) f = LDR_MIN_PERCENT / 100.0f;
    else
      f = 1.0f - (1.0f - LDR_MIN_PERCENT / 100.0f) * (ldrEma - LDR_BRIGHT_RAW) /
                     (float)(LDR_DARK_RAW - LDR_BRIGHT_RAW);
    level = (int)(level * f);
  }
  if (level < 12) level = 12;

  bool setupScreen = screen == Screen::SetupAP || screen == Screen::NeedStops ||
                     screen == Screen::ResetHold || screen == Screen::CalibrateIntro;
  bool night = isNightNow() && !setupScreen && (int32_t)(now - wakeUntil) >= 0;
  if (night) level = settings.nightMode == core::NightMode::Dark ? 0 : 4;
  hw::setBacklightTarget((uint8_t)level);

  if ((int32_t)(now - nextLdrLog) >= 0) {
    nextLdrLog = now + 30000;
    LOGI("LDR raw=%d avg=%d -> backlight %d%s (thresholds bright<=%d dark>=%d)", raw, (int)ldrEma,
         level, night ? " [night]" : "", LDR_BRIGHT_RAW, LDR_DARK_RAW);
  }
}

bool screenIsDark() {
  return hw::backlightLevel() <= 6 && isNightNow() && (int32_t)(millis() - wakeUntil) >= 0;
}

// ------------------------------------------------------------------ touch

void handleTouch(Screen screen) {
  hw::Touch t = hw::pollTouch();
  if (t.down) lastActivity = millis();
  if (t.event == hw::TouchEvent::None) return;

  // At night the first tap only wakes the display.
  if (screenIsDark()) {
    wakeUntil = millis() + kNightWakeMs;
    return;
  }
  if (isNightNow()) wakeUntil = millis() + kNightWakeMs;  // keep awake while used

  if (t.event == hw::TouchEvent::LongPress) {
    if (screen == Screen::ResetConfirm) {
      // Erasing needs a deliberate press-and-hold on the "Erase" button.
      if (ui::hitTestResetConfirm(t.x, t.y) == ui::HitZone::Erase) factoryReset();
    } else if (screen == Screen::NoWifi) {
      SharedLock l;
      g.reqOpenPortal = true;
    } else if (net == NetState::Online) {
      openOverlay(Overlay::SettingsQR);
    }
    return;
  }

  // Tap
  switch (screen) {
    case Screen::Main:
      switch (ui::hitTestMain(vm, t.x, t.y)) {
        case ui::HitZone::Header: openOverlay(Overlay::Weather); break;
        case ui::HitZone::Banner:
          vm.messageIndex = 0;
          vm.messagePage = 0;
          openOverlay(Overlay::Message);
          break;
        default: break;
      }
      break;
    case Screen::MessageDetail: {
      int pages = ui::messagePageCount(renderer.sprite(), vm);
      if (vm.messagePage + 1 < pages) {
        vm.messagePage++;
      } else if (vm.messageIndex + 1 < vm.messageCount) {
        vm.messageIndex++;
        vm.messagePage = 0;
      } else {
        overlay = Overlay::None;
      }
      overlayDeadline = millis() + kDetailTimeoutMs;
      break;
    }
    case Screen::WeatherDetail:
      overlay = Overlay::None;
      break;
    case Screen::SettingsQR:
      if (ui::hitTestSettingsQR(t.x, t.y) == ui::HitZone::ResetButton)
        openOverlay(Overlay::ResetConfirm);
      else overlay = Overlay::None;
      break;
    case Screen::ResetConfirm:
      // A tap on "Erase" only keeps the hint visible; Cancel closes.
      if (ui::hitTestResetConfirm(t.x, t.y) == ui::HitZone::Cancel) overlay = Overlay::None;
      else overlayDeadline = millis() + kQrTimeoutMs;
      break;
    default:
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  LOGI("%s %s starting (panel %s, invert %d)", PRODUCT_NAME, FIRMWARE_VERSION,
       CYD_PANEL_ST7789 ? "ST7789" : "ILI9341", PANEL_INVERT);
  g.mutex = xSemaphoreCreateMutex();

  hw::begin();
  storage::loadSettings(g.settings);
  settings = g.settings;
  settingsVersion = g.settingsVersion;
  if (settings.touchCalValid) hw::lcd.setTouchCalibrate(settings.touchCal);

  if (!renderer.begin(&hw::lcd)) LOGW("band sprite allocation failed!");
  vm.lang = settings.lang;
  vm.screen = Screen::Boot;
  strlcpy(vm.host, PRODUCT_HOSTNAME ".local", sizeof(vm.host));
  render(true);
  hw::setBacklightTarget(settings.brightness * 255 / 100);

  net::start();
  LOGI("setup done, heap %u", (unsigned)ESP.getFreeHeap());
}

void loop() {
  uint32_t now = millis();
  pullShared();
  persistIfDirty();

  bool calib = false, reset = false;
  {
    SharedLock l;
    calib = g.reqCalibrate;
    reset = g.reqFactoryReset;
    g.reqCalibrate = g.reqFactoryReset = false;
  }
  if (reset) factoryReset();
  if (calib) runCalibration();

  handleBootButton();
  if (overlay != Overlay::None && (int32_t)(now - overlayDeadline) >= 0) overlay = Overlay::None;

  Screen screen = bootHold ? Screen::ResetHold : currentScreen();
  handleTouch(screen);
  screen = bootHold ? Screen::ResetHold : currentScreen();

  // Rebuild the view model and render only when something visible may have
  // changed (second tick, new data, navigation, animation step).
  int64_t epoch = (int64_t)time(nullptr);
  bool animated = screen == Screen::Connecting || screen == Screen::NoWifi ||
                  (screen == Screen::Main && !vm.hasDepartureData);
  int anim = animated ? (int)(now / 450) % 3 : 0;
  uint32_t s = sig({(uint32_t)screen, (uint32_t)epoch, dataVersion, settingsVersion,
                    (uint32_t)anim, (uint32_t)vm.messageIndex, (uint32_t)vm.messagePage,
                    (uint32_t)vm.wifiDown, (uint32_t)vm.resetSeconds,
                    (uint32_t)vm.resetReleaseToCalibrate, (uint32_t)net, (uint32_t)timeValid});
  if (s != lastSig) {
    lastSig = s;
    ui::fillFromSnapshot(vm, settings, data, epoch, timeValid);
    vm.screen = screen;
    vm.animPhase = anim;
    render();
  }

  static uint32_t lastBl = 0;
  if (now - lastBl >= 20) {
    lastBl = now;
    static uint32_t lastLevelCalc = 0;
    if (now - lastLevelCalc >= 250) {
      lastLevelCalc = now;
      updateBrightness(screen);
    }
    hw::updateBacklight();
  }
  delay(5);
}

}  // namespace app
