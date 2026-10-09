#include "../app/log.h"
#include "esp_http.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "mvg_parser.h"

namespace {

// Adapts an Arduino Stream. Uses the timed readBytes() so a slow network
// does not look like the end of the data. Keeps a byte count and the last
// bytes read, so a failed parse can be diagnosed from the serial log.
class StreamSource : public core::ByteSource {
 public:
  explicit StreamSource(Stream& s) : s_(s) {}
  int read() override {
    char c;
    if (s_.readBytes(&c, 1) != 1) return -1;
    remember(c);
    return (unsigned char)c;
  }
  size_t readBytes(char* buf, size_t len) override {
    size_t n = s_.readBytes(buf, len);
    for (size_t i = 0; i < n; i++) remember(buf[i]);
    return n;
  }
  size_t total() const { return total_; }
  // Last bytes read, oldest first, non-printable bytes shown as '.'.
  void tail(char* out, size_t size) const {
    size_t n = total_ < kTail ? total_ : kTail;
    if (n > size - 1) n = size - 1;
    for (size_t i = 0; i < n; i++) {
      char c = ring_[(total_ - n + i) % kTail];
      out[i] = (c >= 32 && c < 127) ? c : '.';
    }
    out[n] = 0;
  }

 private:
  static constexpr size_t kTail = 120;
  void remember(char c) { ring_[total_++ % kTail] = c; }
  Stream& s_;
  size_t total_ = 0;
  char ring_[kTail] = {0};
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
  const char* keys[] = {"Transfer-Encoding", "Content-Encoding", "Content-Type"};
  http.collectHeaders(keys, 3);
  int code = http.GET();
  lastStatus_ = code;
  bool ok = false;
  if (code == HTTP_CODE_OK) {
    StreamSource src(http.getStream());
    ok = consume(src);
    if (!ok) {
      char tail[121];
      src.tail(tail, sizeof(tail));
      LOGW("parse failed for %s: %s", url, core::lastParseError());
      LOGW("  read %u bytes, Content-Length %d, Transfer-Encoding '%s', Content-Encoding '%s', "
           "Content-Type '%s'",
           (unsigned)src.total(), http.getSize(), http.header("Transfer-Encoding").c_str(),
           http.header("Content-Encoding").c_str(), http.header("Content-Type").c_str());
      LOGW("  last bytes: %s", tail);
    }
  } else {
    LOGW("HTTP %d for %s", code, url);
  }
  http.end();
  LOGI("  -> %d (%lu ms, heap %u, min %u, stack left %u)", code,
       (unsigned long)(millis() - t0), (unsigned)ESP.getFreeHeap(),
       (unsigned)ESP.getMinFreeHeap(), (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  return ok;
}
