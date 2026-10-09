#include "../app/log.h"
#include "net_task.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <time.h>

#include <algorithm>
#include <vector>

#include "../app/shared.h"
#include "../app/storage.h"
#include "board.h"
#include "board_config.h"
#include "esp_http.h"
#include "providers.h"
#include "web_server.h"

namespace net {

namespace {

constexpr uint32_t kDeparturesEveryMs = 45 * 1000;
constexpr uint32_t kDeparturesRetryMs = 20 * 1000;
constexpr uint32_t kMessagesEveryMs = 5 * 60 * 1000;
constexpr uint32_t kWeatherEveryMs = 15 * 60 * 1000;
constexpr uint32_t kWeatherRetryMs = 2 * 60 * 1000;
constexpr uint32_t kWeatherStaggerMs = 8 * 1000;  // first weather after first departures
constexpr uint32_t kConnectTimeoutMs = 30 * 1000;
constexpr uint32_t kPortalTryTimeoutMs = 20 * 1000;
constexpr uint32_t kPortalLingerMs = 90 * 1000;  // keep AP up so the phone sees "done"
constexpr uint32_t kPortalIdleRestartMs = 10 * 60 * 1000;  // re-opened portal left unused
constexpr const char* kTz = "CET-1CEST,M3.5.0,M10.5.0/3";

DNSServer dns;
EspHttp http;
core::MvgProvider mvg;
core::OpenMeteoProvider meteo;
core::TransportProvider& transport = mvg;
core::WeatherProvider& weatherProvider = meteo;

bool portalActive = false;
uint32_t portalCloseAt = 0;
uint32_t portalOpenedAt = 0;
uint32_t connectStartedAt = 0;
uint32_t portalTryAt = 0;
uint32_t lastReconnectAt = 0;
uint32_t wifiDownSince = 0;
bool servicesStarted = false;

uint32_t nextDepartures = 0, nextMessages = 0, nextWeather = 0, nextHeapLog = 0;
uint32_t seenSettingsVersion = 0;
std::string slotIds[core::kMaxStops];  // which stop each data slot holds

// Large temporaries live here, not on the 16 KB task stack.
core::Settings cfg;
core::StopDepartures tmpStop;
core::StopDepartures tmpAll[core::kMaxStops];
core::ServiceMessage tmpMsgs[core::kMaxMessages];
core::WeatherData tmpWeather;
std::string trySsid, tryPass;  // credentials being tested from the portal

int64_t nowEpoch() { return (int64_t)time(nullptr); }
bool clockValid() { return nowEpoch() > 1700000000; }

void setNet(NetState s) {
  SharedLock l;
  g.net = s;
}

void apName(char* out, size_t n) {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(out, n, "%s-Setup-%02X%02X", PRODUCT_NAME, mac[4], mac[5]);
}

void startPortal() {
  if (portalActive) return;
  char ssid[33];
  apName(ssid, sizeof(ssid));
  bool hadCreds;
  {
    SharedLock l;
    hadCreds = g.hasCredentials;
  }
  if (hadCreds) {
    // Re-opened from "No Wi-Fi": stop STA retries, they hop channels and make
    // the hotspot unstable. Retried after new credentials or a restart.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
  }
  WiFi.mode(WIFI_AP_STA);  // STA needed for scanning
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid);  // open network: joining via QR must be effortless
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  portalActive = true;
  portalCloseAt = 0;
  portalOpenedAt = millis();
  {
    SharedLock l;
    strlcpy(g.apSsid, ssid, sizeof(g.apSsid));
    g.net = NetState::Portal;
    g.portalConnect = PortalConnect::Idle;
    g.scanRequested = true;
  }
  web::setPortalMode(true);
  LOGI("setup portal started: %s", ssid);
}

void stopPortal() {
  if (!portalActive) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  portalActive = false;
  web::setPortalMode(false);
  LOGI("setup portal closed");
}

void beginSta(const std::string& ssid, const std::string& pass) {
  if (!portalActive) WiFi.mode(WIFI_STA);
  WiFi.setHostname(PRODUCT_HOSTNAME);
#ifdef WIFI_TX_POWER
  WiFi.setTxPower(WIFI_TX_POWER);  // lower peak current on weak USB supplies
#endif
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  connectStartedAt = millis();
  lastReconnectAt = millis();
  SharedLock l;
  strlcpy(g.homeSsid, ssid.c_str(), sizeof(g.homeSsid));
}

void onOnline() {
  String ip = WiFi.localIP().toString();
  LOGI("online: %s, RSSI %d", ip.c_str(), WiFi.RSSI());
  if (!servicesStarted) {
    configTzTime(kTz, "pool.ntp.org", "time.google.com", "ptbtime1.ptb.de");
    if (MDNS.begin(PRODUCT_HOSTNAME)) MDNS.addService("http", "tcp", 80);
    servicesStarted = true;
  }
  SharedLock l;
  strlcpy(g.ip, ip.c_str(), sizeof(g.ip));
  g.net = NetState::Online;
  g.wifiDown = false;
  g.rssi = WiFi.RSSI();
}

// ---------------------------------------------------------------- Wi-Fi scan

void handleScan() {
  bool want;
  {
    SharedLock l;
    want = g.scanRequested && !g.scanning;
    if (want) {
      g.scanRequested = false;
      g.scanning = true;
    }
  }
  if (want) WiFi.scanNetworks(true /*async*/, false);

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  bool scanning;
  {
    SharedLock l;
    scanning = g.scanning;
  }
  if (!scanning) return;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  if (n > 0) {
    // de-duplicate by SSID keeping the strongest, then sort by RSSI
    std::vector<int> idx;
    for (int i = 0; i < n; i++) {
      String s = WiFi.SSID(i);
      if (s.isEmpty()) continue;
      bool dup = false;
      for (int& j : idx)
        if (WiFi.SSID(j) == s) {
          dup = true;
          if (WiFi.RSSI(i) > WiFi.RSSI(j)) j = i;
        }
      if (!dup) idx.push_back(i);
    }
    std::sort(idx.begin(), idx.end(), [](int a, int b) { return WiFi.RSSI(a) > WiFi.RSSI(b); });
    for (int i : idx) {
      if (arr.size() >= 20) break;
      JsonObject o = arr.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    }
  }
  WiFi.scanDelete();
  std::string out;
  serializeJson(doc, out);
  SharedLock l;
  g.scanJson = out;
  g.scanning = false;
}

// ---------------------------------------------------------------- Wi-Fi state machine

void wifiLoop() {
  if (portalActive) dns.processNextRequest();
  handleScan();

  // credentials posted from the setup portal
  std::string newSsid, newPass;
  bool forget = false, openPortal = false;
  {
    SharedLock l;
    if (g.credsPending) {
      g.credsPending = false;
      newSsid = g.pendingSsid;
      newPass = g.pendingPass;
      g.pendingPass.clear();
      g.portalConnect = PortalConnect::Trying;
    }
    forget = g.reqForgetWifi;
    g.reqForgetWifi = false;
    openPortal = g.reqOpenPortal;
    g.reqOpenPortal = false;
  }
  if (forget) {
    storage::clearWifi();
    LOGI("Wi-Fi credentials cleared, restarting");
    delay(500);
    ESP.restart();
  }
  if (openPortal) startPortal();
  if (!newSsid.empty()) {
    LOGI("trying new Wi-Fi '%s'", newSsid.c_str());
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP_STA);
    {
      SharedLock l;
      g.hasCredentials = true;
    }
    beginSta(newSsid, newPass);
    portalTryAt = millis();
    trySsid = newSsid;  // saved to NVS only once the connection worked
    tryPass = newPass;
  }

  bool connected = WiFi.status() == WL_CONNECTED;
  NetState state;
  PortalConnect pc;
  {
    SharedLock l;
    state = g.net;
    pc = g.portalConnect;
  }
  uint32_t now = millis();

  if (portalActive && pc == PortalConnect::Trying) {
    if (connected) {
      storage::saveWifi(trySsid, tryPass);
      tryPass.clear();
      onOnline();
      {
        SharedLock l;
        g.portalConnect = PortalConnect::Ok;
      }
      portalCloseAt = now + kPortalLingerMs;
      LOGI("portal: connected, AP closes in %lus", (unsigned long)(kPortalLingerMs / 1000));
    } else if (now - portalTryAt > kPortalTryTimeoutMs) {
      SharedLock l;
      g.portalConnect = PortalConnect::Failed;
      WiFi.disconnect(false, false);
      LOGW("portal: connection failed");
    }
    return;
  }
  if (portalActive && portalCloseAt && now > portalCloseAt) stopPortal();
  if (portalActive && !portalCloseAt && pc == PortalConnect::Idle) {
    bool hadCreds;
    {
      SharedLock l;
      hadCreds = g.hasCredentials;
    }
    if (hadCreds && now - portalOpenedAt > kPortalIdleRestartMs) {
      LOGI("setup portal unused for 10 min, restarting to retry the known Wi-Fi");
      ESP.restart();
    }
  }

  switch (state) {
    case NetState::Portal:
      if (connected && !portalCloseAt) {  // STA came back while the portal was open
        onOnline();
        portalCloseAt = now + 5000;
      }
      break;
    case NetState::Connecting:
      if (connected) onOnline();
      else if (now - connectStartedAt > kConnectTimeoutMs) {
        LOGW("Wi-Fi connect timeout");
        setNet(NetState::Offline);
      }
      break;
    case NetState::Offline:
      if (connected) onOnline();
      else if (now - lastReconnectAt > 20000) {
        lastReconnectAt = now;
        WiFi.reconnect();
      }
      break;
    case NetState::Online: {
      bool down = !connected;
      if (down && !wifiDownSince) wifiDownSince = now;
      if (!down) wifiDownSince = 0;
      if (down && now - wifiDownSince > 5 * 60 * 1000 && now - lastReconnectAt > 60000) {
        lastReconnectAt = now;
        WiFi.reconnect();
      }
      SharedLock l;
      g.wifiDown = down;
      if (!down) g.rssi = WiFi.RSSI();
      break;
    }
    default:
      break;
  }
}

// ---------------------------------------------------------------- fetching

void syncSettings() {
  uint32_t v;
  {
    SharedLock l;
    v = g.settingsVersion;
    if (v == seenSettingsVersion && !g.reqRefresh) return;
    cfg = g.settings;
    g.reqRefresh = false;
  }
  bool changed = v != seenSettingsVersion;
  seenSettingsVersion = v;
  if (changed) {
    // Keep data for stops that survived the edit (matched by id), drop the rest.
    SharedLock l;
    core::StopDepartures* moved = tmpAll;  // scratch, reused by fetchMessages
    std::string newIds[core::kMaxStops];
    for (int i = 0; i < core::kMaxStops; i++) {
      moved[i] = core::StopDepartures();
      if (i < (int)cfg.stops.size()) newIds[i] = cfg.stops[i].id;
      for (int j = 0; j < core::kMaxStops; j++)
        if (!newIds[i].empty() && slotIds[j] == newIds[i]) {
          moved[i] = g.data.stops[j];
          for (int k = 0; k < moved[i].count; k++) moved[i].items[k].stopIndex = (uint8_t)i;
        }
    }
    for (int i = 0; i < core::kMaxStops; i++) {
      g.data.stops[i] = moved[i];
      slotIds[i] = newIds[i];
    }
    g.data.version++;
  }
  uint32_t now = millis();
  nextDepartures = now;  // refetch right away
  nextMessages = now + 3000;
  nextWeather = now + 1500;
}

void fetchDepartures() {
  bool anyError = false;
  int64_t ts = nowEpoch();
  int n = (int)cfg.stops.size();
  for (int i = 0; i < n && i < core::kMaxStops; i++) {
    tmpStop = core::StopDepartures();
    if (transport.fetchDepartures(http, cfg.stops[i], (uint8_t)i, core::kMaxDeparturesPerStop,
                                  tmpStop)) {
      tmpStop.valid = true;
      tmpStop.fetchedAt = ts;
      SharedLock l;
      if (slotIds[i] == cfg.stops[i].id) g.data.stops[i] = tmpStop;
      g.data.version++;
    } else {
      anyError = true;
    }
  }
  SharedLock l;
  g.data.departuresError = anyError;
  if (!anyError && n > 0) g.data.departuresFetchedAt = ts;
  g.data.version++;
}

void fetchMessages() {
  std::vector<core::LineKey> lines;
  {
    SharedLock l;
    for (int i = 0; i < core::kMaxStops; i++) tmpAll[i] = g.data.stops[i];
  }
  lines = core::boardLines(tmpAll, cfg);
  int count = 0;
  if (lines.empty()) {
    SharedLock l;
    g.data.messageCount = 0;
    return;
  }
  if (transport.fetchMessages(http, lines, nowEpoch(), tmpMsgs, core::kMaxMessages, count)) {
    SharedLock l;
    for (int i = 0; i < count; i++) g.data.messages[i] = tmpMsgs[i];
    g.data.messageCount = count;
    g.data.version++;
  }
  // on failure keep the previous messages
}

bool fetchWeather() {
  float lat, lon;
  if (!cfg.weatherLocation(lat, lon)) return true;  // nothing to do yet
  tmpWeather = core::WeatherData();
  bool ok = weatherProvider.fetch(http, lat, lon, nowEpoch(), tmpWeather);
  SharedLock l;
  if (ok) {
    tmpWeather.fetchedAt = nowEpoch();
    g.data.weather = tmpWeather;
  }
  g.data.weatherError = !ok;
  g.data.version++;
  return ok;
}

// ---------------------------------------------------------------- jobs

void runJobs() {
  for (int i = 0; i < kMaxJobs; i++) {
    Job job;
    {
      SharedLock l;
      if (g.jobs[i].state != Job::State::Pending) continue;
      job = g.jobs[i];
    }
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    bool ok = false;
    if (job.type == Job::Type::Search) {
      std::vector<core::StopInfo> found;
      ok = transport.searchStops(http, job.arg.c_str(), found);
      for (const auto& s : found) {
        JsonObject o = arr.add<JsonObject>();
        o["id"] = s.id;
        o["name"] = s.name;
        o["place"] = s.place;
        o["lat"] = s.lat;
        o["lon"] = s.lon;
        o["types"] = s.typeMask;
      }
    } else {
      core::StopConfig st;
      st.id = job.arg;
      tmpStop = core::StopDepartures();
      ok = transport.fetchDepartures(http, st, 0, core::kMaxDeparturesPerStop, tmpStop);
      for (const auto& ld : core::distinctLineDirections(tmpStop)) {
        JsonObject o = arr.add<JsonObject>();
        o["label"] = ld.label;
        o["dest"] = ld.destination;
        o["type"] = core::transportTypeToString(ld.type);
      }
    }
    std::string out;
    serializeJson(doc, out);
    SharedLock l;
    if (g.jobs[i].id == job.id) {
      g.jobs[i].state = ok ? Job::State::Done : Job::State::Error;
      g.jobs[i].result = out;
      g.jobs[i].touchedAt = millis();
    }
  }
}

// ---------------------------------------------------------------- task

void task(void*) {
  std::string ssid, pass;
  bool haveCreds = storage::loadWifi(ssid, pass);
  {
    SharedLock l;
    g.hasCredentials = haveCreds;
  }
  WiFi.persistent(false);  // we store credentials ourselves
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  web::begin();

  if (!haveCreds) {
    startPortal();
  } else {
    setNet(NetState::Connecting);
    beginSta(ssid, pass);
  }

  bool fetchedOnce = false;
  for (;;) {
    bool stop;
    {
      SharedLock l;
      stop = g.shuttingDown;
    }
    if (stop) {  // factory reset: leave Wi-Fi/NVS alone until the restart
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    wifiLoop();
    uint32_t now = millis();
    bool timeOk = clockValid();
    {
      SharedLock l;
      g.timeValid = timeOk;
    }
    bool online = WiFi.status() == WL_CONNECTED;
    if (online) {
      runJobs();
      if (timeOk) {
        syncSettings();
        if (!cfg.stops.empty() && (int32_t)(now - nextDepartures) >= 0) {
          fetchDepartures();
          bool err;
          {
            SharedLock l;
            err = g.data.departuresError;
          }
          nextDepartures = millis() + (err ? kDeparturesRetryMs : kDeparturesEveryMs);
          if (!fetchedOnce) {
            fetchedOnce = true;
            nextWeather = millis() + kWeatherStaggerMs;
            nextMessages = millis() + kWeatherStaggerMs * 2;
          }
        }
        if ((int32_t)(now - nextWeather) >= 0) {
          nextWeather = millis() + (fetchWeather() ? kWeatherEveryMs : kWeatherRetryMs);
        }
        if (!cfg.stops.empty() && (int32_t)(now - nextMessages) >= 0) {
          fetchMessages();
          nextMessages = millis() + kMessagesEveryMs;
        }
      }
    }
    if ((int32_t)(now - nextHeapLog) >= 0) {
      nextHeapLog = now + 60000;
      LOGI("heap free=%u min=%u largest=%u stack_hw=%u", (unsigned)ESP.getFreeHeap(),
            (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
            (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
    vTaskDelay(pdMS_TO_TICKS(portalActive ? 5 : 20));
  }
}

}  // namespace

// 20 KB: the TLS handshake (mbedTLS) runs on this task's stack.
void start() { xTaskCreatePinnedToCore(task, "net", 20480, nullptr, 1, nullptr, 0); }

}  // namespace net
