#include "settings.h"

#include <ArduinoJson.h>

#include <string.h>

#include <algorithm>

namespace core {

bool Settings::weatherLocation(float& lat, float& lon) const {
  if (weatherOverride) {
    lat = weatherLat;
    lon = weatherLon;
    return true;
  }
  for (const auto& st : stops) {
    if (st.lat != 0 || st.lon != 0) {
      lat = st.lat;
      lon = st.lon;
      return true;
    }
  }
  return false;
}

bool Settings::isNight(int m) const {
  if (nightMode == NightMode::Off) return false;
  if (nightStart == nightEnd) return false;
  if (nightStart < nightEnd) return m >= nightStart && m < nightEnd;
  return m >= nightStart || m < nightEnd;  // window wraps midnight
}

static const char* langStr(Lang l) { return l == Lang::En ? "en" : "de"; }
static const char* nightStr(NightMode m) {
  return m == NightMode::Off ? "off" : (m == NightMode::Dark ? "dark" : "dim");
}

static void stopsToJson(JsonArray arr, const std::vector<StopConfig>& stops) {
  for (const auto& st : stops) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = st.id;
    o["name"] = st.name;
    o["label"] = st.label;
    o["lat"] = st.lat;
    o["lon"] = st.lon;
    o["walk"] = st.walkMin;
    o["types"] = st.typeMask;
    JsonArray ex = o["exclude"].to<JsonArray>();
    for (const auto& e : st.excludes) ex.add(e);
  }
}

std::string settingsToJson(const Settings& s, bool includeTouchCal) {
  JsonDocument doc;
  doc["v"] = 1;
  doc["lang"] = langStr(s.lang);
  stopsToJson(doc["stops"].to<JsonArray>(), s.stops);
  JsonObject w = doc["weather"].to<JsonObject>();
  w["override"] = s.weatherOverride;
  w["lat"] = s.weatherLat;
  w["lon"] = s.weatherLon;
  w["name"] = s.weatherName;
  JsonObject d = doc["display"].to<JsonObject>();
  d["brightness"] = s.brightness;
  d["auto"] = s.autoBrightness;
  JsonObject n = doc["night"].to<JsonObject>();
  n["mode"] = nightStr(s.nightMode);
  n["start"] = s.nightStart;
  n["end"] = s.nightEnd;
  if (includeTouchCal && s.touchCalValid) {
    JsonArray c = doc["touchCal"].to<JsonArray>();
    for (uint16_t v : s.touchCal) c.add(v);
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

template <typename T>
static T clampv(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static bool validCoord(float lat, float lon) {
  return lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180 && !(lat == 0 && lon == 0);
}

static void readStops(JsonArrayConst arr, std::vector<StopConfig>& stops) {
  stops.clear();
  for (JsonObjectConst o : arr) {
    if ((int)stops.size() >= kMaxStops) break;
    StopConfig st;
    st.id = o["id"] | "";
    if (st.id.empty()) continue;
    st.name = o["name"] | st.id.c_str();
    st.label = o["label"] | "";
    if (st.label.empty()) st.label = defaultStopLabel(st.name);
    if (st.label.size() > 24) st.label.resize(24);
    st.lat = o["lat"] | 0.0f;
    st.lon = o["lon"] | 0.0f;
    st.walkMin = (uint8_t)clampv<int>(o["walk"] | 0, 0, 60);
    st.typeMask = (uint8_t)(o["types"] | (int)kAllTypes);
    if (st.typeMask == 0) st.typeMask = kAllTypes;
    for (JsonVariantConst e : o["exclude"].as<JsonArrayConst>()) {
      const char* v = e.as<const char*>();
      if (v && *v && st.excludes.size() < 64) st.excludes.emplace_back(v);
    }
    stops.push_back(std::move(st));
  }
}

static void readCommon(JsonObjectConst doc, Settings& s) {
  if (doc["lang"].is<const char*>()) {
    s.lang = strcmp(doc["lang"].as<const char*>(), "en") == 0 ? Lang::En : Lang::De;
  }
  if (doc["stops"].is<JsonArrayConst>()) readStops(doc["stops"].as<JsonArrayConst>(), s.stops);
  JsonObjectConst w = doc["weather"];
  if (!w.isNull()) {
    s.weatherOverride = w["override"] | s.weatherOverride;
    float lat = w["lat"] | s.weatherLat, lon = w["lon"] | s.weatherLon;
    if (validCoord(lat, lon)) {
      s.weatherLat = lat;
      s.weatherLon = lon;
    } else {
      s.weatherOverride = false;
    }
    s.weatherName = w["name"] | s.weatherName.c_str();
  }
  JsonObjectConst d = doc["display"];
  if (!d.isNull()) {
    s.brightness = (uint8_t)clampv<int>(d["brightness"] | (int)s.brightness, 5, 100);
    s.autoBrightness = d["auto"] | s.autoBrightness;
  }
  JsonObjectConst n = doc["night"];
  if (!n.isNull()) {
    const char* m = n["mode"] | nightStr(s.nightMode);
    s.nightMode = strcmp(m, "off") == 0    ? NightMode::Off
                  : strcmp(m, "dark") == 0 ? NightMode::Dark
                                           : NightMode::Dim;
    s.nightStart = (uint16_t)clampv<int>(n["start"] | (int)s.nightStart, 0, 1439);
    s.nightEnd = (uint16_t)clampv<int>(n["end"] | (int)s.nightEnd, 0, 1439);
  }
}

bool settingsFromJson(const char* json, Settings& out) {
  JsonDocument doc;
  if (!json || deserializeJson(doc, json)) return false;
  Settings s;  // start from defaults
  readCommon(doc.as<JsonObjectConst>(), s);
  JsonArrayConst cal = doc["touchCal"];
  if (cal.size() == 8) {
    for (int i = 0; i < 8; i++) s.touchCal[i] = cal[i] | 0;
    s.touchCalValid = true;
  }
  out = std::move(s);
  return true;
}

bool applySettingsUpdate(const char* json, Settings& s) {
  JsonDocument doc;
  if (!json || deserializeJson(doc, json)) return false;
  if (!doc.is<JsonObject>()) return false;
  readCommon(doc.as<JsonObjectConst>(), s);
  return true;
}

std::string defaultStopLabel(const std::string& name) {
  std::string n = name;
  static const char* prefixes[] = {"München, ", "München-", "Muenchen, "};
  for (const char* p : prefixes) {
    size_t pl = strlen(p);
    if (n.size() > pl && n.compare(0, pl, p) == 0) n = n.substr(pl);
  }
  // "Gauting, Bahnhof" -> "Gauting" when too long for the board.
  size_t comma = n.find(", ");
  if (n.size() > 14 && comma != std::string::npos && comma >= 3) n.resize(comma);
  // Hard limit (the board ellipsizes visually anyway), cut at a UTF-8 boundary.
  if (n.size() > 24) {
    size_t cut = 24;
    while (cut > 0 && (n[cut] & 0xC0) == 0x80) cut--;
    n.resize(cut);
    while (!n.empty() && (n.back() == ' ' || n.back() == '-' || n.back() == ',')) n.pop_back();
  }
  return n;
}

}  // namespace core
