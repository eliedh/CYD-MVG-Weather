#include "../app/log.h"
#include "esp_http.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "mvg_parser.h"

namespace {

// Adapts an Arduino Stream. Uses the timed readBytes() so a slow network
// does not look like the end of the data.
class StreamSource : public core::ByteSource {
 public:
  explicit StreamSource(Stream& s) : s_(s) {}
  int read() override {
    char c;
    return s_.readBytes(&c, 1) == 1 ? (unsigned char)c : -1;
  }
  size_t readBytes(char* buf, size_t len) override { return s_.readBytes(buf, len); }

 private:
  Stream& s_;
};

}  // namespace

bool EspHttp::doGet(const char* url, Consumer& consume) {
  if (WiFi.status() != WL_CONNECTED) {
    lastStatus_ = -1;
    return false;
  }
  // A TLS handshake needs ~45 kB. If the heap is that low, other tasks'
  // allocations would fail during it (and abort), so skip this round instead.
  if (ESP.getFreeHeap() < 70000 || ESP.getMaxAllocHeap() < 32000) {
    LOGW("low memory (heap %u, largest %u) - skipping %s", (unsigned)ESP.getFreeHeap(),
         (unsigned)ESP.getMaxAllocHeap(), url);
    lastStatus_ = -3;
    return false;
  }
  uint32_t t0 = millis();
  LOGI("GET %s (heap %u, largest block %u)", url, (unsigned)ESP.getFreeHeap(),
       (unsigned)ESP.getMaxAllocHeap());
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.useHTTP10(true);
  http.setReuse(false);
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  http.setUserAgent("Mozilla/5.0 (compatible; Abfahrt-CYD/1.0)");
  if (!http.begin(client, url)) {
    lastStatus_ = -2;
    return false;
  }
  http.addHeader("Accept", "application/json");
  int code = http.GET();
  lastStatus_ = code;
  bool ok = false;
  if (code == HTTP_CODE_OK) {
    StreamSource src(http.getStream());
    ok = consume(src);
    if (!ok) LOGW("parse failed for %s: %s", url, core::lastParseError());
  } else {
    LOGW("HTTP %d for %s", code, url);
  }
  http.end();
  LOGI("  -> %d (%lu ms, heap %u, min %u, stack left %u)", code,
       (unsigned long)(millis() - t0), (unsigned)ESP.getFreeHeap(),
       (unsigned)ESP.getMinFreeHeap(), (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  return ok;
}
