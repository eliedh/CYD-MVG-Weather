#include "../app/log.h"
#include "esp_http.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "mvg_parser.h"

namespace {

// Reads the response body straight from the network client, in chunks.
//
// Not Stream::readBytes(): NetworkClient declares its own _timeout, so
// HTTPClient::setTimeout() never reaches Stream::_timeout, which stays at the
// Arduino default of 1 s. Large responses (the 380 kB messages feed) pause
// for longer than that after the first ~8 kB, and the stream "ended" there.
// Here we wait up to kStallMs for new data and only stop when the server has
// closed the connection and everything was read.
//
// Also keeps a byte count and the last bytes read for diagnostics.
class ClientSource : public core::ByteSource {
 public:
  explicit ClientSource(NetworkClient& c) : c_(c) {}
  int read() override {
    if (pos_ >= len_ && !fill()) return -1;
    char c = (char)buf_[pos_++];
    ring_[total_++ % kTail] = c;
    return (unsigned char)c;
  }
  size_t total() const { return total_; }
  bool stalled() const { return stalled_; }
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
  static constexpr uint32_t kStallMs = 10000;

  bool fill() {
    uint32_t start = millis();
    for (;;) {
      int avail = c_.available();
      if (avail > 0) {
        int n = c_.read(buf_, avail < (int)sizeof(buf_) ? avail : (int)sizeof(buf_));
        if (n > 0) {
          pos_ = 0;
          len_ = (size_t)n;
          return true;
        }
      } else if (!c_.connected()) {
        return false;  // server closed the connection: real end of data
      }
      if (millis() - start > kStallMs) {
        stalled_ = true;
        return false;
      }
      delay(2);
    }
  }

  NetworkClient& c_;
  uint8_t buf_[512];
  size_t pos_ = 0, len_ = 0;
  size_t total_ = 0;
  bool stalled_ = false;
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
    ClientSource src(http.getStream());
    ok = consume(src);
    if (!ok) {
      char tail[121];
      src.tail(tail, sizeof(tail));
      LOGW("parse failed for %s: %s", url, core::lastParseError());
      LOGW("  read %u bytes%s, Content-Length %d, Transfer-Encoding '%s', Content-Encoding '%s', "
           "Content-Type '%s'",
           (unsigned)src.total(), src.stalled() ? " (stalled >10 s)" : "", http.getSize(), http.header("Transfer-Encoding").c_str(),
           http.header("Content-Encoding").c_str(), http.header("Content-Type").c_str());
      LOGW("  last bytes: %s", tail);
    }
  } else {
    LOGW("HTTP %d for %s", code, url);
  }
  else if (ok && src.total() > 20000) LOGI("  read %u bytes", (unsigned)src.total());
  http.end();
  LOGI("  -> %d (%lu ms, heap %u, min %u, stack left %u)", code,
       (unsigned long)(millis() - t0), (unsigned)ESP.getFreeHeap(),
       (unsigned)ESP.getMinFreeHeap(), (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  return ok;
}
