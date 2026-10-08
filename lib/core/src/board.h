// Departure board logic: merges all stops into one list, applies per-stop
// filters and walking time, and marks the next catchable departure.
#pragma once

#include <vector>

#include "model.h"
#include "mvg_parser.h"
#include "settings.h"

namespace core {

struct BoardRow {
  Departure dep;
  int32_t secondsUntil = 0;  // until (real-time) departure
  int16_t minutes = 0;       // floor(secondsUntil / 60), >= 0
  bool highlight = false;    // the next departure you can still catch
  int16_t leaveInMin = -1;   // minutes until you must leave (only if walk > 0)
  uint8_t walkMin = 0;
};

// True if the stop's transport-type mask or line/direction excludes hide `d`.
bool isFilteredOut(const StopConfig& stop, const Departure& d);

// Builds the merged board. Departures you cannot reach anymore
// (secondsUntil < walk time) or that have already left are dropped.
// Rows are sorted by effective departure time. Returns number of rows.
int buildBoard(const StopDepartures* stops, const Settings& s, int64_t now, BoardRow* out,
               int maxRows);

// Lines currently on the board (deduplicated), used to select service messages.
std::vector<LineKey> boardLines(const StopDepartures* stops, const Settings& s);

// Distinct line/direction pairs of a stop's departures (for the filter UI).
struct LineDirection {
  std::string label;
  std::string destination;
  TransportType type;
};
std::vector<LineDirection> distinctLineDirections(const StopDepartures& d);

}  // namespace core
