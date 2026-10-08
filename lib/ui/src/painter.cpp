#include "painter.h"

#include <string.h>

namespace ui {

void Painter::clear(uint32_t color) { g_.fillScreen(color); }

void Painter::fillRect(int x, int y, int w, int h, uint32_t color) {
  if (!touches(y, h)) return;
  g_.fillRect(x, y - oy_, w, h, color);
}

void Painter::fillRoundRect(int x, int y, int w, int h, int r, uint32_t color) {
  if (!touches(y, h)) return;
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  g_.fillSmoothRoundRect(x, y - oy_, w, h, r, color);
}

void Painter::fillCircle(int x, int y, int r, uint32_t color) {
  if (!touches(y - r - 1, 2 * r + 3)) return;
  g_.fillSmoothCircle(x, y - oy_, r, color);
}

void Painter::line(int x0, int y0, int x1, int y1, float hw, uint32_t color) {
  int top = (y0 < y1 ? y0 : y1) - (int)hw - 2;
  int h = (y0 < y1 ? y1 - y0 : y0 - y1) + 2 * (int)hw + 4;
  if (!touches(top, h)) return;
  g_.drawWideLine(x0, y0 - oy_, x1, y1 - oy_, hw, color);
}

void Painter::hline(int x, int y, int w, uint32_t color) {
  if (!touches(y, 1)) return;
  g_.drawFastHLine(x, y - oy_, w, color);
}

void Painter::triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
  int lo = y0 < y1 ? y0 : y1;
  lo = lo < y2 ? lo : y2;
  int hi = y0 > y1 ? y0 : y1;
  hi = hi > y2 ? hi : y2;
  if (!touches(lo, hi - lo + 1)) return;
  g_.fillTriangle(x0, y0 - oy_, x1, y1 - oy_, x2, y2 - oy_, color);
}

void Painter::arc(int x, int y, int r0, int r1, float a0, float a1, uint32_t color) {
  int r = r0 > r1 ? r0 : r1;
  if (!touches(y - r, 2 * r + 1)) return;
  g_.fillArc(x, y - oy_, r0, r1, a0, a1, color);
}

void Painter::setClip(int x, int y, int w, int h) { g_.setClipRect(x, y - oy_, w, h); }
void Painter::clearClip() { g_.clearClipRect(); }

int Painter::fontHeight(const lgfx::IFont* font) {
  g_.setFont(font);
  return g_.fontHeight();
}

int Painter::textWidth(const char* s, const lgfx::IFont* font) {
  if (!s || !*s) return 0;
  return g_.textWidth(s, font);
}

int Painter::text(const char* s, int x, int y, const lgfx::IFont* font, uint32_t color,
                  textdatum_t datum) {
  if (!s || !*s) return 0;
  g_.setFont(font);
  int h = g_.fontHeight();
  // Conservative vertical reject: the text occupies at most [y-h, y+h].
  if (!touches(y - h - 2, 2 * h + 4)) return g_.textWidth(s);
  g_.setTextColor(color);
  g_.setTextDatum(datum);
  g_.setTextWrap(false, false);
  return (int)g_.drawString(s, x, y - oy_);
}

static size_t prevBoundary(const std::string& s, size_t pos) {
  if (pos == 0) return 0;
  pos--;
  while (pos > 0 && ((unsigned char)s[pos] & 0xC0) == 0x80) pos--;
  return pos;
}

std::string Painter::ellipsize(const char* s, const lgfx::IFont* font, int maxW) {
  if (!s) return std::string();
  std::string str(s);
  if (textWidth(str.c_str(), font) <= maxW) return str;
  static const char* kEll = "\xE2\x80\xA6";  // …
  size_t end = str.size();
  while (end > 0) {
    end = prevBoundary(str, end);
    std::string t = str.substr(0, end);
    while (!t.empty() && (t.back() == ' ' || t.back() == ',' || t.back() == '-')) t.pop_back();
    t += kEll;
    if (textWidth(t.c_str(), font) <= maxW) return t;
  }
  return kEll;
}

std::vector<std::string> Painter::wrap(const char* s, const lgfx::IFont* font, int maxW) {
  std::vector<std::string> lines;
  if (!s) return lines;
  std::string line;
  const char* p = s;
  auto flush = [&]() {
    lines.push_back(line);
    line.clear();
  };
  while (*p) {
    if (*p == '\n') {
      flush();
      p++;
      continue;
    }
    // next word (including leading spaces)
    while (*p == ' ') p++;
    const char* ws = p;
    while (*p && *p != ' ' && *p != '\n') p++;
    std::string word(ws, p - ws);
    if (word.empty()) continue;
    std::string candidate = line.empty() ? word : line + " " + word;
    if (textWidth(candidate.c_str(), font) <= maxW) {
      line = candidate;
      continue;
    }
    if (!line.empty()) flush();
    // hard-break words that are longer than a line
    while (textWidth(word.c_str(), font) > maxW) {
      size_t cut = word.size();
      while (cut > 1 && textWidth(word.substr(0, cut).c_str(), font) > maxW)
        cut = prevBoundary(word, cut);
      if (cut == 0) cut = 1;
      lines.push_back(word.substr(0, cut));
      word = word.substr(cut);
    }
    line = word;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

}  // namespace ui
