// Everything a screen needs to draw itself. Filled by the firmware's app
// layer (from the shared snapshot) or by the host preview (from fixtures),
// so rendering is fully separated from hardware and networking.
#pragma once

#include <time.h>

#include "board.h"
#include "model.h"
#include "settings.h"

namespace ui {

enum class Screen : uint8_t {
  Boot,
  SetupAP,         // hotspot + QR to join it (bilingual)
  Connecting,      // joining home Wi-Fi
  NoWifi,          // home Wi-Fi unreachable
  NeedStops,       // online but no stops configured: QR to settings page
  Main,            // weather header + departures
  WeatherDetail,   // hourly forecast
  MessageDetail,   // service message, paged
  SettingsQR,      // long-press: settings URL + QR (+ reset button)
  ResetConfirm,    // "reset everything?" - Cancel / hold Erase
  ResetHold,       // BOOT button held: countdown
  CalibrateIntro,  // explains the touch calibration that follows
  Notice,          // short centred message (saved, restarting, ...)
};

constexpr int kMaxBoardRows = 8;

struct ViewModel {
  Screen screen = Screen::Boot;
  core::Lang lang = core::Lang::De;

  // time
  int64_t now = 0;
  bool timeValid = false;
  struct tm local = {};

  // network
  char apSsid[33] = {0};
  char homeSsid[33] = {0};
  char ip[16] = {0};
  char host[40] = {0};  // "abfahrt.local"
  bool wifiDown = false;

  // weather
  core::WeatherData weather;

  // departures
  core::BoardRow rows[kMaxBoardRows];
  int rowCount = 0;
  bool hasDepartureData = false;  // at least one stop fetched successfully
  bool departuresError = false;   // last fetch round failed
  int64_t departuresFetchedAt = 0;
  int stopCount = 0;
  char stopLabels[core::kMaxStops][25] = {{0}};

  // service messages relevant to displayed lines
  // Points into the caller's DataSnapshot (no copy - RAM is tight); valid
  // while that snapshot lives.
  const core::ServiceMessage* messages = nullptr;
  int messageCount = 0;
  int messageIndex = 0;
  int messagePage = 0;

  // misc
  int animPhase = 0;     // increments for the activity dots
  int resetSeconds = 0;  // ResetHold countdown
  bool resetReleaseToCalibrate = false;
  bool resetReleaseToErase = false;
  const char* noticeTitle = nullptr;
  const char* noticeBody = nullptr;
};

// Age (minutes) of the departure data when it should be flagged as stale,
// otherwise -1.
int staleMinutes(const ViewModel& vm);

}  // namespace ui
