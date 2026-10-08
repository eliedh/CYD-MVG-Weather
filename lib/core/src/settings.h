// User settings (everything except Wi-Fi credentials). Persisted as one JSON
// document in NVS by the firmware; (de)serialisation lives here so it is testable.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "model.h"

namespace core {

enum class Lang : uint8_t { De = 0, En = 1 };

enum class NightMode : uint8_t { Off = 0, Dim = 1, Dark = 2 };

struct StopConfig {
  std::string id;      // MVG globalId
  std::string name;    // full name for the settings page
  std::string label;   // short label shown on the board when >1 stop
  float lat = 0, lon = 0;
  uint8_t walkMin = 0;           // walking time from home to the stop
  uint8_t typeMask = kAllTypes;  // allowed transport types (bit per TransportType)
  // Hidden lines/directions. "U3" hides the whole line, "U3>Fürstenried West"
  // hides one direction. Everything not listed is shown, so new short-turn
  // destinations appear automatically.
  std::vector<std::string> excludes;
};

struct Settings {
  Lang lang = Lang::De;
  std::vector<StopConfig> stops;

  // Weather location override (default: first stop's coordinates).
  bool weatherOverride = false;
  float weatherLat = 48.1374f, weatherLon = 11.5755f;  // Marienplatz
  std::string weatherName;

  uint8_t brightness = 80;  // percent, daytime maximum
  bool autoBrightness = true;

  NightMode nightMode = NightMode::Dim;
  uint16_t nightStart = 22 * 60;  // minutes after midnight
  uint16_t nightEnd = 6 * 60 + 30;

  bool touchCalValid = false;
  uint16_t touchCal[8] = {0};

  // Returns where the weather should be fetched for; false if unknown.
  bool weatherLocation(float& lat, float& lon) const;
  bool isNight(int minutesOfDay) const;
};

// JSON round-trip. fromJson is tolerant: missing fields keep defaults,
// out-of-range values are clamped. Returns false only on malformed JSON.
std::string settingsToJson(const Settings& s, bool includeTouchCal = true);
bool settingsFromJson(const char* json, Settings& out);

// Applies a (partial) update coming from the settings web page. Unknown keys
// are ignored, so the page cannot corrupt calibration data.
bool applySettingsUpdate(const char* json, Settings& s);

// "München, Marienplatz" -> "Marienplatz"; keeps it short for the board.
std::string defaultStopLabel(const std::string& name);

}  // namespace core
