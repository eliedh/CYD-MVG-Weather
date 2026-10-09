#include "mvg_parser.h"

#include <ArduinoJson.h>
#include <string.h>

#include "text_util.h"

namespace core {

static char g_parseError[64] = "";
const char* lastParseError() { return g_parseError; }

int64_t toEpochSeconds(int64_t v) {
  // Anything beyond year ~5000 in seconds is certainly milliseconds.
  return v > 100000000000LL ? v / 1000 : v;
}

TransportType transportTypeFromString(const char* s) {
  if (!s) return TransportType::Other;
  struct M {
    const char* n;
    TransportType t;
  };
  static const M map[] = {
      {"UBAHN", TransportType::UBahn},       {"SBAHN", TransportType::SBahn},
      {"TRAM", TransportType::Tram},         {"BUS", TransportType::Bus},
      {"REGIONAL_BUS", TransportType::RegionalBus}, {"REGIONALBUS", TransportType::RegionalBus},
      {"BAHN", TransportType::Train},        {"TRAIN", TransportType::Train},
      {"SCHIFF", TransportType::Ship},       {"SHIP", TransportType::Ship},
      {"U-BAHN", TransportType::UBahn},      {"S-BAHN", TransportType::SBahn},
  };
  for (const M& m : map)
    if (strcasecmp(s, m.n) == 0) return m.t;
  return TransportType::Other;
}

const char* transportTypeToString(TransportType t) {
  switch (t) {
    case TransportType::UBahn: return "UBAHN";
    case TransportType::SBahn: return "SBAHN";
    case TransportType::Tram: return "TRAM";
    case TransportType::Bus: return "BUS";
    case TransportType::RegionalBus: return "REGIONAL_BUS";
    case TransportType::Train: return "BAHN";
    case TransportType::Ship: return "SCHIFF";
    default: return "OTHER";
  }
}

template <typename Doc, typename Filter, typename Fn>
bool forEachArrayElement(ByteSource& in, Doc& doc, const Filter& filter, Fn&& fn) {
  // Find the opening '[' of the first array (tolerates a wrapping object such
  // as {"departures":[...]} from older API versions).
  g_parseError[0] = 0;
  int c = in.nextNonSpace();
  if (c == '[') {
    c = in.nextNonSpace();
    if (c == ']') return true;  // empty array
  } else if (c == '{') {
    // Wrapped: use the first non-empty array of objects.
    do {
      if (!in.skipPast('[')) {
        snprintf(g_parseError, sizeof(g_parseError), "object without a non-empty array");
        return false;
      }
      c = in.nextNonSpace();
    } while (c != '{');
  } else {
    snprintf(g_parseError, sizeof(g_parseError), "no JSON array found (starts with '%c', %d)",
             c > 31 && c < 127 ? c : '?', c);
    return false;
  }
  // We consumed the first char of the first element; push it back by
  // replaying it through a tiny adapter.
  struct Replay : ByteSource {
    ByteSource& src;
    int first;
    Replay(ByteSource& s, int f) : src(s), first(f) {}
    int read() override {
      if (first >= 0) {
        int r = first;
        first = -1;
        return r;
      }
      return src.read();
    }
    size_t readBytes(char* buf, size_t len) override {
      if (!len) return 0;
      if (first >= 0) {
        buf[0] = (char)first;
        first = -1;
        return 1 + src.readBytes(buf + 1, len - 1);
      }
      return src.readBytes(buf, len);
    }
  } replay(in, c);

  ByteSource* src = &replay;
  int index = 0;
  for (;;) {
    doc.clear();
    DeserializationError err =
        deserializeJson(doc, *src, DeserializationOption::Filter(filter),
                        DeserializationOption::NestingLimit(12));
    if (err) {
      snprintf(g_parseError, sizeof(g_parseError), "element %d: %s", index, err.c_str());
      return false;
    }
    if (doc.template is<JsonObject>()) fn(doc.template as<JsonObjectConst>());
    index++;
    int sep = src->nextNonSpace();
    if (sep == ',') continue;
    if (sep != ']')
      snprintf(g_parseError, sizeof(g_parseError), "element %d: unexpected '%c' (%d) after it",
               index - 1, sep > 31 && sep < 127 ? sep : '?', sep);
    return sep == ']';
  }
}

// --- helpers for tolerant field access -----------------------------------

static const char* str(JsonObjectConst o, const char* a, const char* b = nullptr,
                       const char* c = nullptr) {
  for (const char* k : {a, b, c}) {
    if (!k) continue;
    JsonVariantConst v = o[k];
    if (v.is<const char*>()) return v.as<const char*>();
  }
  return nullptr;
}

static int64_t num(JsonObjectConst o, const char* a, const char* b = nullptr,
                   const char* c = nullptr) {
  for (const char* k : {a, b, c}) {
    if (!k) continue;
    JsonVariantConst v = o[k];
    if (v.is<int64_t>()) return v.as<int64_t>();
    if (v.is<double>()) return (int64_t)v.as<double>();
  }
  return 0;
}

// --- locations --------------------------------------------------------------

bool parseLocations(ByteSource& in, std::vector<StopInfo>& out, size_t maxResults) {
  JsonDocument filter;
  for (const char* k : {"type", "name", "place", "globalId", "id", "latitude", "longitude",
                        "transportTypes"})
    filter[k] = true;
  JsonDocument doc;
  out.clear();
  return forEachArrayElement(in, doc, filter, [&](JsonObjectConst o) {
    if (out.size() >= maxResults) return;
    const char* type = str(o, "type");
    if (type && strcasecmp(type, "STATION") != 0) return;
    const char* id = str(o, "globalId", "id");
    const char* name = str(o, "name");
    if (!id || !name) return;
    StopInfo s;
    s.id = id;
    s.name = name;
    if (const char* p = str(o, "place")) s.place = p;
    s.lat = o["latitude"] | 0.0f;
    s.lon = o["longitude"] | 0.0f;
    for (JsonVariantConst t : o["transportTypes"].as<JsonArrayConst>())
      s.typeMask |= typeBit(transportTypeFromString(t.as<const char*>()));
    out.push_back(std::move(s));
  });
}

// --- departures ------------------------------------------------------------

bool parseDepartures(ByteSource& in, uint8_t stopIndex, StopDepartures& out) {
  JsonDocument filter;
  for (const char* k :
       {"plannedDepartureTime", "realtimeDepartureTime", "departureTimePlanned",
        "departureTime", "realtime", "realTime", "delayInMinutes", "delay", "transportType",
        "product", "label", "destination", "direction", "cancelled", "isCancelled", "platform"})
    filter[k] = true;
  filter["line"]["label"] = true;
  filter["line"]["transportType"] = true;

  JsonDocument doc;
  out.count = 0;
  return forEachArrayElement(in, doc, filter, [&](JsonObjectConst o) {
    if (out.count >= kMaxDeparturesPerStop) return;
    Departure d;
    d.stopIndex = stopIndex;
    d.plannedTime = toEpochSeconds(num(o, "plannedDepartureTime", "departureTimePlanned"));
    int64_t rt = toEpochSeconds(num(o, "realtimeDepartureTime", "departureTime"));
    d.realtime = (o["realtime"] | (o["realTime"] | false));
    d.delayMin = (int16_t)num(o, "delayInMinutes", "delay");
    if (d.plannedTime == 0) d.plannedTime = rt;
    if (d.plannedTime == 0) return;  // unusable
    if (rt == 0 && d.realtime) rt = d.plannedTime + d.delayMin * 60;
    d.realtimeTime = rt;
    if (d.realtime && rt && !o["delayInMinutes"].is<int>() && !o["delay"].is<int>())
      d.delayMin = (int16_t)((rt - d.plannedTime) / 60);
    d.cancelled = o["cancelled"] | (o["isCancelled"] | false);

    JsonObjectConst line = o["line"];
    const char* type = str(o, "transportType", "product");
    if (!type && !line.isNull()) type = str(line, "transportType");
    d.type = transportTypeFromString(type);
    const char* label = str(o, "label");
    if (!label && !line.isNull()) label = str(line, "label");
    copyUtf8(d.label, sizeof(d.label), label ? label : "?");
    copyUtf8(d.destination, sizeof(d.destination), str(o, "destination", "direction"));

    JsonVariantConst pl = o["platform"];
    if (pl.is<int>()) snprintf(d.platform, sizeof(d.platform), "%d", pl.as<int>());
    else if (pl.is<const char*>()) copyUtf8(d.platform, sizeof(d.platform), pl.as<const char*>());

    out.items[out.count++] = d;
  });
}

// --- messages --------------------------------------------------------------

static bool parseMessagesRaw(ByteSource& in, const std::vector<LineKey>& lines, int64_t now,
                             ServiceMessage* out, int maxOut, int& count);

bool parseMessages(ByteSource& in, const std::vector<LineKey>& lines, int64_t now,
                   ServiceMessage* out, int maxOut, int& count) {
  bool ok = parseMessagesRaw(in, lines, now, out, maxOut, count);
  // Incidents first, otherwise keep the feed's order (insertion sort: stable,
  // no allocation, at most kMaxMessages elements).
  for (int i = 1; i < count; i++) {
    for (int j = i; j > 0 && out[j].incident && !out[j - 1].incident; j--) {
      ServiceMessage tmp = out[j];
      out[j] = out[j - 1];
      out[j - 1] = tmp;
    }
  }
  return ok;
}

static bool parseMessagesRaw(ByteSource& in, const std::vector<LineKey>& lines, int64_t now,
                             ServiceMessage* out, int maxOut, int& count) {
  JsonDocument filter;
  for (const char* k : {"title", "description", "text", "validFrom", "validTo", "type"})
    filter[k] = true;
  filter["lines"][0]["label"] = true;
  filter["lines"][0]["transportType"] = true;

  JsonDocument doc;
  count = 0;
  return forEachArrayElement(in, doc, filter, [&](JsonObjectConst o) {
    // (no early exit when full: a later INCIDENT may still replace a
    // schedule change)
    int64_t from = toEpochSeconds(num(o, "validFrom"));
    int64_t to = toEpochSeconds(num(o, "validTo"));
    if (from && now && now < from) return;
    if (to && now && now > to) return;

    char matched[48] = {0};
    size_t mlen = 0;
    for (JsonObjectConst l : o["lines"].as<JsonArrayConst>()) {
      const char* label = l["label"];
      if (!label) continue;
      TransportType lt = transportTypeFromString(l["transportType"] | "");
      for (const LineKey& k : lines) {
        if (strcasecmp(k.label, label) != 0) continue;
        if (lt != TransportType::Other && k.type != TransportType::Other && lt != k.type) continue;
        if (strstr(matched, label)) break;  // already listed
        int n = snprintf(matched + mlen, sizeof(matched) - mlen, "%s%s", mlen ? ", " : "", label);
        if (n > 0 && mlen + n < sizeof(matched)) mlen += n;
        break;
      }
    }
    if (!mlen) return;

    const char* title = str(o, "title");
    if (!title || !*title) return;
    char tclean[sizeof(out[0].title)];
    htmlToText(title, tclean, sizeof(tclean));
    for (int i = 0; i < count; i++)
      if (strcmp(out[i].title, tclean) == 0) return;  // duplicate (same text per line)

    // Incidents (actual disruptions) win over the many long-running schedule
    // changes: when full, an incident replaces the last schedule change.
    const char* type = str(o, "type");
    bool incident = type && strcasecmp(type, "INCIDENT") == 0;
    int slot = -1;
    if (count < maxOut) {
      slot = count++;
    } else if (incident) {
      for (int i = count - 1; i >= 0; i--)
        if (!out[i].incident) {
          slot = i;
          break;
        }
    }
    if (slot < 0) return;
    ServiceMessage& m = out[slot];
    m = ServiceMessage();
    m.incident = incident;
    copyUtf8(m.title, sizeof(m.title), tclean);
    htmlToText(str(o, "description", "text"), m.text, sizeof(m.text));
    copyUtf8(m.lines, sizeof(m.lines), matched);
    m.validFrom = from;
    m.validTo = to;
  });
}

}  // namespace core
