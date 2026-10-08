// Data provider interfaces. The firmware only talks to these, so the MVG API
// can be replaced (e.g. by the MVV EFA API) without touching UI or app logic.
#pragma once

#include <string>
#include <vector>

#include "byte_source.h"
#include "model.h"
#include "mvg_parser.h"
#include "settings.h"

namespace core {

class TransportProvider {
 public:
  virtual ~TransportProvider() = default;
  virtual const char* name() const = 0;
  virtual bool searchStops(HttpFetcher& http, const char* query, std::vector<StopInfo>& out) = 0;
  virtual bool fetchDepartures(HttpFetcher& http, const StopConfig& stop, uint8_t stopIndex,
                               int limit, StopDepartures& out) = 0;
  virtual bool fetchMessages(HttpFetcher& http, const std::vector<LineKey>& lines, int64_t now,
                             ServiceMessage* out, int maxOut, int& count) = 0;
};

class WeatherProvider {
 public:
  virtual ~WeatherProvider() = default;
  virtual bool fetch(HttpFetcher& http, float lat, float lon, int64_t now, WeatherData& out) = 0;
};

// mvg.de "bgw-pt/v3" (unofficial, no key; covers MVV regional stops too).
class MvgProvider : public TransportProvider {
 public:
  const char* name() const override { return "MVG"; }
  bool searchStops(HttpFetcher& http, const char* query, std::vector<StopInfo>& out) override;
  bool fetchDepartures(HttpFetcher& http, const StopConfig& stop, uint8_t stopIndex, int limit,
                       StopDepartures& out) override;
  bool fetchMessages(HttpFetcher& http, const std::vector<LineKey>& lines, int64_t now,
                     ServiceMessage* out, int maxOut, int& count) override;

  static std::string searchUrl(const char* query);
  static std::string departuresUrl(const StopConfig& stop, int limit);
  static std::string messagesUrl();
};

class OpenMeteoProvider : public WeatherProvider {
 public:
  bool fetch(HttpFetcher& http, float lat, float lon, int64_t now, WeatherData& out) override;
};

}  // namespace core
