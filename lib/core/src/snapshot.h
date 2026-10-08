// The data the network task produces and the UI consumes. The firmware keeps
// one instance behind a mutex and the UI copies it before rendering.
#pragma once

#include "model.h"

namespace core {

struct DataSnapshot {
  StopDepartures stops[kMaxStops];
  bool departuresError = false;     // last round had at least one failure
  int64_t departuresFetchedAt = 0;  // last fully successful round

  WeatherData weather;
  bool weatherError = false;

  ServiceMessage messages[kMaxMessages];
  int messageCount = 0;

  uint32_t version = 0;  // bumped on every change
};

}  // namespace core
