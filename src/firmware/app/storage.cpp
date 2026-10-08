#include "log.h"
#include "storage.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

namespace storage {

static const char* kNs = "abfahrt";

bool loadSettings(core::Settings& out) {
  Preferences p;
  if (!p.begin(kNs, true)) return false;
  String json = p.getString("cfg", "");
  p.end();
  if (json.isEmpty()) return false;
  bool ok = core::settingsFromJson(json.c_str(), out);
  if (!ok) LOGW("stored settings are corrupt, using defaults");
  return ok;
}

bool saveSettings(const core::Settings& s) {
  std::string json = core::settingsToJson(s);
  Preferences p;
  if (!p.begin(kNs, false)) return false;
  size_t n = p.putString("cfg", json.c_str());
  p.end();
  LOGI("settings saved (%u bytes)", (unsigned)json.size());
  return n == json.size();
}

bool loadWifi(std::string& ssid, std::string& pass) {
  Preferences p;
  if (!p.begin(kNs, true)) return false;
  ssid = p.getString("ssid", "").c_str();
  pass = p.getString("pass", "").c_str();
  p.end();
  return !ssid.empty();
}

bool saveWifi(const std::string& ssid, const std::string& pass) {
  Preferences p;
  if (!p.begin(kNs, false)) return false;
  p.putString("ssid", ssid.c_str());
  p.putString("pass", pass.c_str());
  p.end();
  return true;
}

void clearWifi() {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.remove("ssid");
  p.remove("pass");
  p.end();
}

void factoryReset() {
  Preferences p;
  if (p.begin(kNs, false)) {
    p.clear();
    p.end();
  }
  WiFi.disconnect(true, true);  // also erase any credentials the core cached
}

}  // namespace storage
