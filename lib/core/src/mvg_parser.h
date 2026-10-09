// Parsers for the (unofficial) MVG API used by mvg.de, v3 "bgw-pt".
// Responses are top-level JSON arrays; they are parsed one element at a time
// with ArduinoJson filters so memory use is bounded by a single element, not
// the response size (the messages feed can be several hundred kB).
#pragma once

#include <vector>

#include "byte_source.h"
#include "model.h"

namespace core {

struct LineKey {
  char label[8] = {0};
  TransportType type = TransportType::Other;
};

// Stop search ("locations?query=..&locationTypes=STATION").
bool parseLocations(ByteSource& in, std::vector<StopInfo>& out, size_t maxResults = 8);

// Departures for one stop. Fills out.items/out.count (does not touch fetchedAt).
bool parseDepartures(ByteSource& in, uint8_t stopIndex, StopDepartures& out);

// Service messages. Keeps only messages currently valid at `now` that name
// at least one of `lines`. Returns false only if the stream is not JSON.
bool parseMessages(ByteSource& in, const std::vector<LineKey>& lines, int64_t now,
                   ServiceMessage* out, int maxOut, int& count);

// Human-readable reason for the last failed parse in this module (for logs),
// e.g. "element 3: NoMemory" or "no JSON array found (starts with '<')".
const char* lastParseError();

// Epoch value that may be in milliseconds or seconds -> seconds.
int64_t toEpochSeconds(int64_t v);

}  // namespace core
