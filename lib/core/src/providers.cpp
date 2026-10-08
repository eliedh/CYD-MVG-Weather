#include "providers.h"

#include <stdio.h>

#include "text_util.h"
#include "weather_parser.h"

namespace core {

static const char* kMvgBase = "https://www.mvg.de/api/bgw-pt/v3";

std::string MvgProvider::searchUrl(const char* query) {
  return std::string(kMvgBase) + "/locations?query=" + urlEncode(query) +
         "&locationTypes=STATION";
}

std::string MvgProvider::departuresUrl(const StopConfig& stop, int limit) {
  // Only send transportTypes when the stop is restricted; the full list is the
  // API default. SCHIFF is never sent (not every API version accepts it) -
  // filtering happens locally anyway.
  std::string types;
  bool restricted = false;
  for (int t = 0; t < (int)TransportType::Ship; t++) {
    if (!(stop.typeMask & (1u << t))) {
      restricted = true;
      continue;
    }
    if (!types.empty()) types += ',';
    types += transportTypeToString((TransportType)t);
  }
  if (!restricted) types.clear();
  char lim[8];
  snprintf(lim, sizeof(lim), "%d", limit);
  std::string url = std::string(kMvgBase) + "/departures?globalId=" + urlEncode(stop.id.c_str()) +
                    "&limit=" + lim;
  if (!types.empty()) url += "&transportTypes=" + types;
  return url;
}

std::string MvgProvider::messagesUrl() { return std::string(kMvgBase) + "/messages"; }

bool MvgProvider::searchStops(HttpFetcher& http, const char* query, std::vector<StopInfo>& out) {
  std::string url = searchUrl(query);
  return http.get(url.c_str(), [&](ByteSource& s) { return parseLocations(s, out); });
}

bool MvgProvider::fetchDepartures(HttpFetcher& http, const StopConfig& stop, uint8_t stopIndex,
                                  int limit, StopDepartures& out) {
  std::string url = departuresUrl(stop, limit);
  return http.get(url.c_str(),
                  [&](ByteSource& s) { return parseDepartures(s, stopIndex, out); });
}

bool MvgProvider::fetchMessages(HttpFetcher& http, const std::vector<LineKey>& lines, int64_t now,
                                ServiceMessage* out, int maxOut, int& count) {
  std::string url = messagesUrl();
  return http.get(url.c_str(), [&](ByteSource& s) {
    return parseMessages(s, lines, now, out, maxOut, count);
  });
}

bool OpenMeteoProvider::fetch(HttpFetcher& http, float lat, float lon, int64_t now,
                              WeatherData& out) {
  std::string url = openMeteoUrl(lat, lon);
  return http.get(url.c_str(), [&](ByteSource& s) { return parseOpenMeteo(s, now, out); });
}

}  // namespace core
