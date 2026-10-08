#include "../app/log.h"
#include "web_server.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "../app/shared.h"
#include "../web/web_assets.h"
#include "board_config.h"

namespace web {

namespace {

AsyncWebServer server(80);
volatile bool portalMode = false;
constexpr size_t kMaxBody = 8192;

void sendGz(AsyncWebServerRequest* r, const uint8_t* data, size_t len) {
  AsyncWebServerResponse* res = r->beginResponse(200, "text/html; charset=utf-8", data, len);
  res->addHeader("Content-Encoding", "gzip");
  res->addHeader("Cache-Control", "no-store");
  r->send(res);
}

void sendJson(AsyncWebServerRequest* r, const std::string& json, int code = 200) {
  AsyncWebServerResponse* res = r->beginResponse(code, "application/json", json.c_str());
  res->addHeader("Cache-Control", "no-store");
  r->send(res);
}

void sendOk(AsyncWebServerRequest* r) { sendJson(r, "{\"ok\":true}"); }
void sendErr(AsyncWebServerRequest* r, int code, const char* msg) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  std::string s;
  serializeJson(d, s);
  sendJson(r, s, code);
}

void redirectToPortal(AsyncWebServerRequest* r) {
  r->redirect("http://192.168.4.1/");
}

// Collects a request body into request->_tempObject (freed by the library).
void collectBody(AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
  if (total > kMaxBody) return;
  if (index == 0) {
    r->_tempObject = malloc(total + 1);
    if (!r->_tempObject) return;
  }
  if (!r->_tempObject) return;
  memcpy((uint8_t*)r->_tempObject + index, data, len);
  if (index + len == total) ((char*)r->_tempObject)[total] = 0;
}

const char* body(AsyncWebServerRequest* r) { return (const char*)r->_tempObject; }

const char* netStateName(NetState s) {
  switch (s) {
    case NetState::Portal: return "portal";
    case NetState::Connecting: return "connecting";
    case NetState::Offline: return "offline";
    case NetState::Online: return "online";
    default: return "init";
  }
}

int createJob(Job::Type type, const char* arg) {
  SharedLock l;
  uint32_t now = millis();
  int slot = -1;
  for (int i = 0; i < kMaxJobs; i++) {
    Job& j = g.jobs[i];
    bool stale = now - j.touchedAt > 60000;
    if (j.state == Job::State::Free || (j.state != Job::State::Pending && stale) ||
        (j.state == Job::State::Pending && now - j.touchedAt > 120000)) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return -1;
  Job& j = g.jobs[slot];
  j.id = g.nextJobId++;
  j.type = type;
  j.state = Job::State::Pending;
  j.arg = arg;
  j.result.clear();
  j.touchedAt = now;
  return (int)j.id;
}

void handleInfo(AsyncWebServerRequest* r) {
  JsonDocument d;
  {
    SharedLock l;
    d["portal"] = portalMode;
    d["net"] = netStateName(g.net);
    d["ssid"] = g.homeSsid;
    d["ap"] = g.apSsid;
    d["ip"] = g.ip;
    d["host"] = PRODUCT_HOSTNAME ".local";
    d["rssi"] = g.rssi;
    d["timeValid"] = g.timeValid;
    d["lang"] = g.settings.lang == core::Lang::En ? "en" : "de";
    d["lastUpdate"] = g.data.departuresFetchedAt;
    d["apiError"] = g.data.departuresError;
  }
  d["version"] = FIRMWARE_VERSION;
  d["heap"] = ESP.getFreeHeap();
  d["uptime"] = millis() / 1000;
  std::string s;
  serializeJson(d, s);
  sendJson(r, s);
}

void routes() {
  // ---------------------------------------------------------------- pages
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (portalMode) sendGz(r, WEB_SETUP_GZ, WEB_SETUP_GZ_LEN);
    else sendGz(r, WEB_SETTINGS_GZ, WEB_SETTINGS_GZ_LEN);
  });
  server.on("/setup", HTTP_GET,
            [](AsyncWebServerRequest* r) { sendGz(r, WEB_SETUP_GZ, WEB_SETUP_GZ_LEN); });
  server.on("/settings", HTTP_GET,
            [](AsyncWebServerRequest* r) { sendGz(r, WEB_SETTINGS_GZ, WEB_SETTINGS_GZ_LEN); });

  // Captive-portal detection endpoints of Android, iOS/macOS, Windows, Firefox.
  for (const char* path : {"/generate_204", "/gen_204", "/hotspot-detect.html",
                           "/library/test/success.html", "/connecttest.txt", "/ncsi.txt",
                           "/redirect", "/fwlink", "/canonical.html", "/success.txt"}) {
    server.on(path, HTTP_ANY, [](AsyncWebServerRequest* r) {
      if (portalMode) redirectToPortal(r);
      else r->send(204);
    });
  }
  server.onNotFound([](AsyncWebServerRequest* r) {
    if (portalMode) redirectToPortal(r);
    else r->send(404, "text/plain", "not found");
  });

  // ---------------------------------------------------------------- shared API
  server.on("/api/info", HTTP_GET, handleInfo);

  // ---------------------------------------------------------------- setup portal API
  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    std::string s;
    {
      SharedLock l;
      if (r->hasParam("refresh")) g.scanRequested = true;
      s = std::string("{\"scanning\":") + ((g.scanning || g.scanRequested) ? "true" : "false") +
          ",\"networks\":" + g.scanJson + "}";
    }
    sendJson(r, s);
  });

  server.on(
      "/api/wifi", HTTP_POST,
      [](AsyncWebServerRequest* r) {
        JsonDocument d;
        if (!body(r) || deserializeJson(d, body(r))) return sendErr(r, 400, "bad json");
        const char* ssid = d["ssid"] | "";
        const char* pass = d["pass"] | "";
        if (!*ssid || strlen(ssid) > 32 || strlen(pass) > 63) return sendErr(r, 400, "bad ssid");
        SharedLock l;
        g.pendingSsid = ssid;
        g.pendingPass = pass;
        g.credsPending = true;
        g.portalConnect = PortalConnect::Trying;
        const char* lang = d["lang"] | "";
        if (*lang) {
          g.settings.lang = strcmp(lang, "en") == 0 ? core::Lang::En : core::Lang::De;
          g.settingsDirty = true;
          g.settingsVersion++;
        }
        sendOk(r);
      },
      nullptr, collectBody);

  server.on("/api/wifistatus", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    {
      SharedLock l;
      const char* st = "idle";
      if (g.portalConnect == PortalConnect::Trying) st = "trying";
      else if (g.portalConnect == PortalConnect::Ok) st = "ok";
      else if (g.portalConnect == PortalConnect::Failed) st = "failed";
      d["state"] = st;
      d["ip"] = g.ip;
      d["ssid"] = g.homeSsid;
    }
    d["host"] = PRODUCT_HOSTNAME ".local";
    std::string s;
    serializeJson(d, s);
    sendJson(r, s);
  });

  // ---------------------------------------------------------------- settings API
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* r) {
    std::string s;
    {
      SharedLock l;
      s = core::settingsToJson(g.settings, false);
    }
    sendJson(r, s);
  });

  server.on(
      "/api/settings", HTTP_POST,
      [](AsyncWebServerRequest* r) {
        if (!body(r)) return sendErr(r, 400, "empty body");
        SharedLock l;
        core::Settings next = g.settings;
        if (!core::applySettingsUpdate(body(r), next)) return sendErr(r, 400, "bad json");
        g.settings = next;
        g.settingsVersion++;
        g.settingsDirty = true;
        std::string s = core::settingsToJson(g.settings, false);
        sendJson(r, s);
      },
      nullptr, collectBody);

  server.on("/api/search", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!r->hasParam("q")) return sendErr(r, 400, "missing q");
    String q = r->getParam("q")->value();
    q.trim();
    if (q.length() < 2 || q.length() > 60) return sendErr(r, 400, "query length");
    int id = createJob(Job::Type::Search, q.c_str());
    if (id < 0) return sendErr(r, 503, "busy");
    sendJson(r, "{\"job\":" + std::to_string(id) + "}");
  });

  server.on("/api/lines", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!r->hasParam("id")) return sendErr(r, 400, "missing id");
    String id = r->getParam("id")->value();
    if (id.length() < 3 || id.length() > 40) return sendErr(r, 400, "bad id");
    int job = createJob(Job::Type::Lines, id.c_str());
    if (job < 0) return sendErr(r, 503, "busy");
    sendJson(r, "{\"job\":" + std::to_string(job) + "}");
  });

  server.on("/api/job", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!r->hasParam("id")) return sendErr(r, 400, "missing id");
    uint32_t id = (uint32_t)r->getParam("id")->value().toInt();
    std::string s;
    {
      SharedLock l;
      for (Job& j : g.jobs) {
        if (j.id != id || j.state == Job::State::Free) continue;
        j.touchedAt = millis();
        if (j.state == Job::State::Pending) s = "{\"state\":\"pending\"}";
        else if (j.state == Job::State::Error) s = "{\"state\":\"error\",\"result\":[]}";
        else s = "{\"state\":\"done\",\"result\":" + j.result + "}";
        if (j.state != Job::State::Pending) j.state = Job::State::Free;
        break;
      }
    }
    if (s.empty()) return sendErr(r, 404, "unknown job");
    sendJson(r, s);
  });

  auto flag = [](const char* path, bool Shared::*member) {
    server.on(path, HTTP_POST, [member](AsyncWebServerRequest* r) {
      {
        SharedLock l;
        g.*member = true;
      }
      sendOk(r);
    });
  };
  flag("/api/calibrate", &Shared::reqCalibrate);
  flag("/api/reset", &Shared::reqFactoryReset);
  flag("/api/forget-wifi", &Shared::reqForgetWifi);
  flag("/api/refresh", &Shared::reqRefresh);
}

}  // namespace

void begin() {
  routes();
  server.begin();
  LOGI("web server started");
}

void setPortalMode(bool on) { portalMode = on; }

}  // namespace web
