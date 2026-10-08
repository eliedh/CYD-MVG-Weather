#include "board.h"

#include <string.h>

#include <algorithm>

namespace core {

bool isFilteredOut(const StopConfig& stop, const Departure& d) {
  if (!(stop.typeMask & typeBit(d.type))) return true;
  for (const std::string& ex : stop.excludes) {
    size_t sep = ex.find('>');
    if (sep == std::string::npos) {
      if (strcasecmp(ex.c_str(), d.label) == 0) return true;
    } else {
      if (ex.compare(0, sep, d.label) == 0 && strlen(d.label) == sep &&
          strcmp(ex.c_str() + sep + 1, d.destination) == 0)
        return true;
    }
  }
  return false;
}

static int64_t floorDiv(int64_t a, int64_t b) {
  int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
  return q;
}

int buildBoard(const StopDepartures* stops, const Settings& s, int64_t now, BoardRow* out,
               int maxRows) {
  std::vector<BoardRow> rows;
  rows.reserve(kMaxStops * kMaxDeparturesPerStop);
  int nStops = std::min((int)s.stops.size(), kMaxStops);
  for (int i = 0; i < nStops; i++) {
    const StopConfig& sc = s.stops[i];
    const StopDepartures& sd = stops[i];
    if (!sd.valid) continue;
    for (int j = 0; j < sd.count; j++) {
      const Departure& d = sd.items[j];
      if (isFilteredOut(sc, d)) continue;
      int64_t until = d.effectiveTime() - now;
      // Departed already, or you would not make it there in time.
      if (until < 0 || until < (int64_t)sc.walkMin * 60) continue;
      BoardRow r;
      r.dep = d;
      r.dep.stopIndex = (uint8_t)i;
      r.secondsUntil = (int32_t)until;
      r.minutes = (int16_t)floorDiv(until, 60);
      r.walkMin = sc.walkMin;
      rows.push_back(r);
    }
  }
  std::stable_sort(rows.begin(), rows.end(), [](const BoardRow& a, const BoardRow& b) {
    int64_t ta = a.dep.effectiveTime(), tb = b.dep.effectiveTime();
    if (ta != tb) return ta < tb;
    if (a.dep.stopIndex != b.dep.stopIndex) return a.dep.stopIndex < b.dep.stopIndex;
    return strcmp(a.dep.label, b.dep.label) < 0;
  });
  // Drop exact duplicates (same trip reported twice, e.g. overlapping stops).
  rows.erase(std::unique(rows.begin(), rows.end(),
                         [](const BoardRow& a, const BoardRow& b) {
                           return a.dep.stopIndex == b.dep.stopIndex &&
                                  a.dep.plannedTime == b.dep.plannedTime &&
                                  strcmp(a.dep.label, b.dep.label) == 0 &&
                                  strcmp(a.dep.destination, b.dep.destination) == 0;
                         }),
             rows.end());

  for (BoardRow& r : rows) {
    if (r.dep.cancelled) continue;
    r.highlight = true;
    if (r.walkMin > 0)
      r.leaveInMin = (int16_t)floorDiv(r.secondsUntil - (int64_t)r.walkMin * 60, 60);
    break;
  }

  int n = std::min((int)rows.size(), maxRows);
  for (int i = 0; i < n; i++) out[i] = rows[i];
  return n;
}

std::vector<LineKey> boardLines(const StopDepartures* stops, const Settings& s) {
  std::vector<LineKey> keys;
  int nStops = std::min((int)s.stops.size(), kMaxStops);
  for (int i = 0; i < nStops; i++) {
    if (!stops[i].valid) continue;
    for (int j = 0; j < stops[i].count; j++) {
      const Departure& d = stops[i].items[j];
      if (isFilteredOut(s.stops[i], d)) continue;
      bool dup = false;
      for (const LineKey& k : keys)
        if (k.type == d.type && strcmp(k.label, d.label) == 0) dup = true;
      if (dup) continue;
      LineKey k;
      memcpy(k.label, d.label, sizeof(k.label));
      k.type = d.type;
      keys.push_back(k);
    }
  }
  return keys;
}

std::vector<LineDirection> distinctLineDirections(const StopDepartures& sd) {
  std::vector<LineDirection> v;
  for (int j = 0; j < sd.count; j++) {
    const Departure& d = sd.items[j];
    bool dup = false;
    for (const auto& e : v)
      if (e.label == d.label && e.destination == d.destination) dup = true;
    if (!dup) v.push_back({d.label, d.destination, d.type});
  }
  std::sort(v.begin(), v.end(), [](const LineDirection& a, const LineDirection& b) {
    if (a.type != b.type) return a.type < b.type;
    // natural-ish order: shorter labels first ("U3" < "U6" < "U63")
    if (a.label.size() != b.label.size()) return a.label.size() < b.label.size();
    if (a.label != b.label) return a.label < b.label;
    return a.destination < b.destination;
  });
  return v;
}

}  // namespace core
