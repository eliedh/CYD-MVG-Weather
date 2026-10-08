// Minimal byte-stream abstraction so the parsers work on an Arduino Stream
// (HTTPClient) on the device and on in-memory fixtures on the host.
// It satisfies ArduinoJson's custom "Reader" concept (read + readBytes).
#pragma once

#include <stddef.h>
#include <string.h>

#include <string>

namespace core {

class ByteSource {
 public:
  virtual ~ByteSource() = default;
  // Returns next byte (0..255) or -1 on end of stream / timeout.
  virtual int read() = 0;
  virtual size_t readBytes(char* buf, size_t len) {
    size_t n = 0;
    while (n < len) {
      int c = read();
      if (c < 0) break;
      buf[n++] = (char)c;
    }
    return n;
  }

  // Skip bytes until `c` has been consumed. Returns false at end of stream.
  bool skipPast(char c) {
    for (;;) {
      int r = read();
      if (r < 0) return false;
      if (r == c) return true;
    }
  }

  // Skip whitespace and return the next significant character (consumed), or -1.
  int nextNonSpace() {
    for (;;) {
      int r = read();
      if (r < 0) return -1;
      if (r != ' ' && r != '\n' && r != '\r' && r != '\t') return r;
    }
  }
};

class MemorySource : public ByteSource {
 public:
  MemorySource(const char* data, size_t len) : data_(data), len_(len) {}
  explicit MemorySource(const std::string& s) : data_(s.data()), len_(s.size()) {}
  int read() override { return pos_ < len_ ? (unsigned char)data_[pos_++] : -1; }
  size_t readBytes(char* buf, size_t len) override {
    size_t n = len_ - pos_ < len ? len_ - pos_ : len;
    memcpy(buf, data_ + pos_, n);
    pos_ += n;
    return n;
  }

 private:
  const char* data_;
  size_t len_;
  size_t pos_ = 0;
};

// Abstract HTTP GET used by the providers. The device implementation wraps
// HTTPClient/WiFiClientSecure; tests serve fixtures.
class HttpFetcher {
 public:
  virtual ~HttpFetcher() = default;
  // Performs a GET; on HTTP 200 calls `consume` with the body stream and
  // returns its result. Returns false on transport errors / non-200.
  template <typename F>
  bool get(const char* url, F&& consume) {
    struct Thunk : Consumer {
      F& f;
      explicit Thunk(F& fn) : f(fn) {}
      bool operator()(ByteSource& s) override { return f(s); }
    } thunk(consume);
    return doGet(url, thunk);
  }
  int lastStatus() const { return lastStatus_; }

 protected:
  struct Consumer {
    virtual bool operator()(ByteSource& s) = 0;
    virtual ~Consumer() = default;
  };
  virtual bool doGet(const char* url, Consumer& consume) = 0;
  int lastStatus_ = 0;
};

}  // namespace core
