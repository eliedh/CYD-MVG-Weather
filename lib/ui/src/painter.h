// Draws in *screen* coordinates into a horizontal band sprite. Every screen is
// written as if it painted the full 320x240 display; the painter shifts by the
// band's top edge and skips primitives that do not touch the band.
#pragma once

#include <LovyanGFX.hpp>
#include <string>
#include <vector>

namespace ui {

class Painter {
 public:
  Painter(lgfx::LovyanGFX& gfx, int bandTop, int bandH) : g_(gfx), oy_(bandTop), bh_(bandH) {}

  lgfx::LovyanGFX& gfx() { return g_; }
  int bandTop() const { return oy_; }
  int bandBottom() const { return oy_ + bh_; }
  bool touches(int y, int h) const { return y < oy_ + bh_ && y + h > oy_; }

  void clear(uint32_t color);
  void fillRect(int x, int y, int w, int h, uint32_t color);
  void fillRoundRect(int x, int y, int w, int h, int r, uint32_t color);  // anti-aliased
  void fillCircle(int x, int y, int r, uint32_t color);                   // anti-aliased
  void line(int x0, int y0, int x1, int y1, float halfWidth, uint32_t color);  // AA wide line
  void hline(int x, int y, int w, uint32_t color);
  void triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);
  void arc(int x, int y, int r0, int r1, float a0, float a1, uint32_t color);
  void setClip(int x, int y, int w, int h);
  void clearClip();

  // Text (UTF-8). Drawn without a background colour so glyph edges blend into
  // whatever is already in the sprite. Returns the drawn width.
  int text(const char* s, int x, int y, const lgfx::IFont* font, uint32_t color,
           textdatum_t datum = textdatum_t::baseline_left);
  int textWidth(const char* s, const lgfx::IFont* font);
  int fontHeight(const lgfx::IFont* font);

  // Shortens with a trailing "…" so the result fits into maxW pixels.
  std::string ellipsize(const char* s, const lgfx::IFont* font, int maxW);
  // Greedy word wrap (honours '\n'); long words are hard-broken.
  std::vector<std::string> wrap(const char* s, const lgfx::IFont* font, int maxW);

 private:
  lgfx::LovyanGFX& g_;
  int oy_, bh_;
};

}  // namespace ui
