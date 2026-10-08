#include "text_util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace core {

static size_t utf8SeqLen(unsigned char c) {
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;  // invalid lead byte: treat as single
}

void copyUtf8(char* dst, size_t dstSize, const char* src) {
  if (!dst || dstSize == 0) return;
  dst[0] = 0;
  if (!src) return;
  size_t o = 0;
  while (*src) {
    size_t n = utf8SeqLen((unsigned char)*src);
    if (o + n >= dstSize) break;
    for (size_t i = 0; i < n; i++) {
      if (!src[i]) {  // truncated sequence in source
        dst[o] = 0;
        return;
      }
      dst[o++] = src[i];
    }
    src += n;
  }
  dst[o] = 0;
}

uint32_t nextCodePoint(const char*& p) {
  const unsigned char* s = (const unsigned char*)p;
  uint32_t cp;
  size_t n = utf8SeqLen(*s);
  if (n == 1) {
    cp = *s;
    if (cp >= 0x80) cp = 0xFFFD;
  } else {
    cp = *s & (0xFF >> (n + 1));
    for (size_t i = 1; i < n; i++) {
      if ((s[i] & 0xC0) != 0x80) {
        p += i;
        return 0xFFFD;
      }
      cp = (cp << 6) | (s[i] & 0x3F);
    }
  }
  p += n;
  return cp;
}

size_t appendUtf8(char* dst, size_t room, uint32_t cp) {
  char tmp[4];
  size_t n;
  if (cp < 0x80) {
    tmp[0] = (char)cp;
    n = 1;
  } else if (cp < 0x800) {
    tmp[0] = (char)(0xC0 | (cp >> 6));
    tmp[1] = (char)(0x80 | (cp & 0x3F));
    n = 2;
  } else if (cp < 0x10000) {
    tmp[0] = (char)(0xE0 | (cp >> 12));
    tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    tmp[2] = (char)(0x80 | (cp & 0x3F));
    n = 3;
  } else {
    tmp[0] = (char)(0xF0 | (cp >> 18));
    tmp[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    tmp[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    tmp[3] = (char)(0x80 | (cp & 0x3F));
    n = 4;
  }
  if (n > room) return 0;
  memcpy(dst, tmp, n);
  return n;
}

namespace {

struct Entity {
  const char* name;
  uint32_t cp;
};

const Entity kEntities[] = {
    {"amp", '&'},      {"lt", '<'},        {"gt", '>'},        {"quot", '"'},
    {"apos", '\''},    {"nbsp", ' '},      {"auml", 0xE4},     {"ouml", 0xF6},
    {"uuml", 0xFC},    {"Auml", 0xC4},     {"Ouml", 0xD6},     {"Uuml", 0xDC},
    {"szlig", 0xDF},   {"eacute", 0xE9},   {"egrave", 0xE8},   {"agrave", 0xE0},
    {"ndash", 0x2013}, {"mdash", 0x2014},  {"hellip", 0x2026}, {"bdquo", 0x201E},
    {"ldquo", 0x201C}, {"rdquo", 0x201D},  {"lsquo", 0x2018},  {"rsquo", 0x2019},
    {"sbquo", 0x201A}, {"euro", 0x20AC},   {"deg", 0xB0},      {"middot", 0xB7},
    {"rarr", 0x2192},  {"shy", 0xAD},      {"bull", 0x2022},
};

bool isBlockTag(const char* tag, size_t len) {
  static const char* kBlock[] = {"p", "/p", "br", "br/", "li", "/li", "div", "/div",
                                 "ul", "/ul", "h1", "h2", "h3", "h4", "/h1", "/h2",
                                 "/h3", "/h4", "tr", "/tr"};
  for (const char* b : kBlock) {
    size_t bl = strlen(b);
    if (bl == len && strncasecmp(tag, b, len) == 0) return true;
  }
  return false;
}

}  // namespace

void htmlToText(const char* html, char* out, size_t maxBytes) {
  if (!out || maxBytes == 0) return;
  out[0] = 0;
  if (!html) return;
  size_t o = 0;
  // pending: 0 none, 1 space, 2 newline
  int pending = 0;
  bool any = false;
  const char* p = html;

  auto emit = [&](uint32_t cp) -> bool {
    if (cp == 0xAD || cp == 0x200B || cp == 0x200C || cp == 0x200D || cp == 0xFEFF) return true;
    if (cp == '\r' || cp == '\n' || cp == '\t' || cp == ' ' || cp == 0xA0) {
      if (pending < 1) pending = 1;
      return true;
    }
    if (pending && any) {
      char sep = pending == 2 ? '\n' : ' ';
      if (o + 1 >= maxBytes) return false;
      out[o++] = sep;
    }
    pending = 0;
    size_t n = appendUtf8(out + o, maxBytes - 1 - o, cp);
    if (n == 0) return false;
    o += n;
    any = true;
    return true;
  };

  while (*p) {
    if (*p == '<') {
      const char* end = strchr(p, '>');
      if (!end) break;
      const char* t = p + 1;
      while (*t == ' ') t++;
      size_t len = 0;
      while (t + len < end && !isspace((unsigned char)t[len]) && t[len] != '>') len++;
      if (isBlockTag(t, len)) pending = 2;
      p = end + 1;
      continue;
    }
    if (*p == '&') {
      const char* semi = strchr(p, ';');
      if (semi && semi - p <= 10) {
        uint32_t cp = 0;
        if (p[1] == '#') {
          cp = (p[2] == 'x' || p[2] == 'X') ? (uint32_t)strtoul(p + 3, nullptr, 16)
                                            : (uint32_t)strtoul(p + 2, nullptr, 10);
        } else {
          for (const Entity& e : kEntities) {
            size_t nl = strlen(e.name);
            if ((size_t)(semi - p - 1) == nl && strncmp(p + 1, e.name, nl) == 0) {
              cp = e.cp;
              break;
            }
          }
        }
        if (cp) {
          if (!emit(cp)) break;
          p = semi + 1;
          continue;
        }
      }
    }
    uint32_t cp = nextCodePoint(p);
    if (!emit(cp)) break;
  }
  out[o] = 0;
}

std::string urlEncode(const char* s) {
  static const char* hex = "0123456789ABCDEF";
  std::string r;
  if (!s) return r;
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      r += (char)c;
    } else {
      r += '%';
      r += hex[c >> 4];
      r += hex[c & 15];
    }
  }
  return r;
}

bool equalsIgnoreCase(const char* a, const char* b) {
  if (!a || !b) return false;
  return strcasecmp(a, b) == 0;
}

}  // namespace core
