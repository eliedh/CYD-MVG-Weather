// Shared data model. Plain structs with fixed-size buffers so snapshots can be
// copied between the network task and the UI task without heap churn.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace core {

constexpr int kMaxStops = 4;
constexpr int kMaxDeparturesPerStop = 20;
constexpr int kMaxMessages = 4;
constexpr int kMaxHours = 8;

enum class TransportType : uint8_t {
  UBahn = 0,
  SBahn,
  Tram,
  Bus,
  RegionalBus,
  Train,  // regional/long-distance trains (DB "BAHN")
  Ship,
  Other,
  Count
};

// Bit mask helpers for per-stop transport type filters.
inline uint8_t typeBit(TransportType t) { return (uint8_t)(1u << (uint8_t)t); }
constexpr uint8_t kAllTypes = 0xFF;

TransportType transportTypeFromString(const char* s);
const char* transportTypeToString(TransportType t);  // API spelling, e.g. "UBAHN"

struct Departure {
  int64_t plannedTime = 0;   // epoch seconds
  int64_t realtimeTime = 0;  // epoch seconds, 0 = unknown
  int16_t delayMin = 0;
  bool realtime = false;     // true when realtimeTime comes from live data
  bool cancelled = false;
  TransportType type = TransportType::Other;
  uint8_t stopIndex = 0;     // index into Settings::stops
  char label[8] = {0};       // "U3", "S8", "N40" ...
  char destination[48] = {0};
  char platform[8] = {0};

  int64_t effectiveTime() const {
    return (realtime && realtimeTime > 0) ? realtimeTime : plannedTime;
  }
};

struct StopDepartures {
  bool valid = false;        // at least one successful fetch
  int64_t fetchedAt = 0;     // epoch seconds of last success
  uint8_t count = 0;
  Departure items[kMaxDeparturesPerStop];
};

struct StopInfo {  // a search result
  std::string id;      // globalId, e.g. "de:09162:2"
  std::string name;    // "Marienplatz"
  std::string place;   // "München"
  float lat = 0, lon = 0;
  uint8_t typeMask = 0;
};

struct ServiceMessage {
  char title[96] = {0};
  char text[640] = {0};         // tags stripped, whitespace collapsed
  char lines[48] = {0};         // affected displayed lines, "U3, U6"
  int64_t validFrom = 0, validTo = 0;
};

enum class WeatherIcon : uint8_t {
  Clear,
  MostlyClear,
  PartlyCloudy,
  Overcast,
  Fog,
  Drizzle,
  Rain,
  Showers,
  Snow,
  Thunder,
  Unknown
};

struct HourForecast {
  int64_t time = 0;
  float temp = 0;
  uint8_t precipProb = 0;
  uint8_t code = 0;   // WMO weather code
  bool isDay = true;
};

struct WeatherData {
  bool valid = false;
  int64_t fetchedAt = 0;
  float temp = 0, feelsLike = 0;
  uint8_t code = 0;
  bool isDay = true;
  float tMax = 0, tMin = 0;
  uint8_t precipMaxToday = 0;
  uint8_t rainNextHours = 0;  // max precipitation probability over the next 6 h
  float windKmh = 0;
  int64_t sunrise = 0, sunset = 0;
  uint8_t nHours = 0;
  HourForecast hours[kMaxHours];
};

WeatherIcon weatherIconForCode(uint8_t wmo);

}  // namespace core
