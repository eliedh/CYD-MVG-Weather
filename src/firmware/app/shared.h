// State shared between the UI loop (core 1), the network task (core 0) and the
// async web server (AsyncTCP task). Everything is guarded by one mutex; hold it
// only for copies, never across network I/O or rendering.
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <string>

#include "settings.h"
#include "snapshot.h"

enum class NetState : uint8_t {
  Init,        // booting
  Portal,      // setup hotspot active (no credentials, or user re-opened setup)
  Connecting,  // joining the home Wi-Fi
  Offline,     // credentials known but the network is unreachable (still retrying)
  Online,      // connected (wifiDown flags temporary drops)
};

enum class PortalConnect : uint8_t { Idle, Trying, Ok, Failed };

// Background jobs requested by the settings page (they need HTTPS, which must
// not run inside the async web server's task).
struct Job {
  enum class Type : uint8_t { Search, Lines };
  enum class State : uint8_t { Free, Pending, Done, Error };
  uint32_t id = 0;
  Type type = Type::Search;
  State state = State::Free;
  std::string arg;
  std::string result;  // JSON
  uint32_t touchedAt = 0;
};
constexpr int kMaxJobs = 3;

struct Shared {
  SemaphoreHandle_t mutex = nullptr;

  core::Settings settings;
  uint32_t settingsVersion = 1;
  bool settingsDirty = false;  // save to NVS (done by the UI loop)

  core::DataSnapshot data;

  // network status
  NetState net = NetState::Init;
  bool wifiDown = false;
  bool timeValid = false;
  bool hasCredentials = false;
  char apSsid[33] = {0};
  char homeSsid[33] = {0};
  char ip[16] = {0};
  int rssi = 0;

  // portal
  PortalConnect portalConnect = PortalConnect::Idle;
  bool credsPending = false;
  std::string pendingSsid, pendingPass;
  std::string scanJson = "[]";
  bool scanRequested = false;
  bool scanning = false;

  // requests (set by web server or UI, consumed by UI loop / net task)
  bool reqCalibrate = false;
  bool reqFactoryReset = false;
  bool reqForgetWifi = false;
  bool reqOpenPortal = false;
  bool reqRefresh = false;

  Job jobs[kMaxJobs];
  uint32_t nextJobId = 1;
};

extern Shared g;

struct SharedLock {
  SharedLock() { xSemaphoreTake(g.mutex, portMAX_DELAY); }
  ~SharedLock() { xSemaphoreGive(g.mutex); }
  SharedLock(const SharedLock&) = delete;
  SharedLock& operator=(const SharedLock&) = delete;
};
