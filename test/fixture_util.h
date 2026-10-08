// Shared helpers for native tests.
#pragma once
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "byte_source.h"

static const int64_t FIXTURE_NOW = 1791475200;  // 2026-10-08 18:00 CEST

inline std::string readFixture(const std::string& name) {
  const char* dirs[] = {"test/fixtures/", "../test/fixtures/", "../../test/fixtures/"};
  for (const char* d : dirs) {
    std::ifstream f(std::string(d) + name, std::ios::binary);
    if (f) {
      std::stringstream ss;
      ss << f.rdbuf();
      return ss.str();
    }
  }
  return std::string();
}

inline bool fixtureExists(const std::string& name) { return !readFixture(name).empty(); }

// Feeds data in small chunks with readBytes() returning short reads, like a
// network stream would.
class ChunkedSource : public core::ByteSource {
 public:
  explicit ChunkedSource(const std::string& s, size_t chunk = 7) : s_(s), chunk_(chunk) {}
  int read() override { return pos_ < s_.size() ? (unsigned char)s_[pos_++] : -1; }
  size_t readBytes(char* buf, size_t len) override {
    size_t n = std::min(len, std::min(chunk_, s_.size() - pos_));
    memcpy(buf, s_.data() + pos_, n);
    pos_ += n;
    return n;
  }

 private:
  const std::string& s_;
  size_t chunk_;
  size_t pos_ = 0;
};
