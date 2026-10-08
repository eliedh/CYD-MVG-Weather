#include "widgets.h"

#include <string.h>

#include <vector>

#include "fonts.h"
#include "theme.h"
#include "lgfx/utility/lgfx_qrcode.h"

namespace ui {
namespace widgets {

using namespace theme;

static uint32_t dim(uint32_t c) {
  // blend 55 % towards the background
  auto mix = [](uint32_t a, uint32_t b) { return (a * 45 + b * 55) / 100; };
  uint32_t r = mix((c >> 16) & 0xFF, (kBg >> 16) & 0xFF);
  uint32_t g = mix((c >> 8) & 0xFF, (kBg >> 8) & 0xFF);
  uint32_t b = mix(c & 0xFF, kBg & 0xFF);
  return (r << 16) | (g << 8) | b;
}

int lineBadge(Painter& p, int x, int y, int w, int h, core::TransportType type,
              const char* label, bool dimmed) {
  core::LineStyle st = core::lineStyle(type, label);
  const lgfx::IFont* f = fonts::badge();
  int tw = p.textWidth(label, f);
  if (tw + 10 > w) w = tw + 10;
  uint32_t bg = dimmed ? dim(st.bg) : st.bg;
  uint32_t bg2 = dimmed ? dim(st.bg2) : st.bg2;
  uint32_t fg = dimmed ? dim(st.fg) : st.fg;
  int r = st.shape == core::BadgeShape::Pill ? h / 2 : (st.shape == core::BadgeShape::Rect ? 3 : 7);
  p.fillRoundRect(x, y, w, h, r, bg);
  if (bg2 != bg) {  // split badge (U7, U8): right half in the second colour
    p.setClip(x + w / 2, y, w - w / 2, h);
    p.fillRoundRect(x, y, w, h, r, bg2);
    p.clearClip();
  }
  p.text(label, x + w / 2, y + h / 2 + 1, f, fg, textdatum_t::middle_center);
  return w;
}

namespace {
struct QrCache {
  std::string text;
  int size = 0;  // modules per side
  std::vector<uint8_t> modules;
};
QrCache g_qr[2];
int g_qrNext = 0;

const QrCache* qrFor(const char* text) {
  for (auto& c : g_qr)
    if (c.size && c.text == text) return &c;
  QrCache& c = g_qr[g_qrNext];
  g_qrNext = (g_qrNext + 1) % 2;
  c.text = text;
  c.size = 0;
  c.modules.clear();
  for (uint8_t version = 2; version <= 10; version++) {
    QRCode qr;
    std::vector<uint8_t> buf(lgfx_qrcode_getBufferSize(version));
    if (lgfx_qrcode_initText(&qr, buf.data(), version, 1 /*ECC M*/, text) != 0) continue;
    c.size = qr.size;
    c.modules.resize(qr.size * qr.size);
    for (int yy = 0; yy < qr.size; yy++)
      for (int xx = 0; xx < qr.size; xx++)
        c.modules[yy * qr.size + xx] = lgfx_qrcode_getModule(&qr, xx, yy) ? 1 : 0;
    break;
  }
  return c.size ? &c : nullptr;
}
}  // namespace

void qrCard(Painter& p, const char* text, int x, int y, int size) {
  if (!p.touches(y, size)) return;
  p.fillRoundRect(x, y, size, size, 10, 0xFFFFFF);
  const QrCache* q = qrFor(text);
  if (!q) return;
  int quiet = 2;  // modules of quiet zone (card radius adds more)
  int scale = size / (q->size + 2 * quiet);
  if (scale < 1) scale = 1;
  int px = q->size * scale;
  int ox = x + (size - px) / 2, oy = y + (size - px) / 2;
  for (int yy = 0; yy < q->size; yy++) {
    int sy = oy + yy * scale;
    if (!p.touches(sy, scale)) continue;
    int run = -1;
    for (int xx = 0; xx <= q->size; xx++) {
      bool on = xx < q->size && q->modules[yy * q->size + xx];
      if (on && run < 0) run = xx;
      if (!on && run >= 0) {
        p.fillRect(ox + run * scale, sy, (xx - run) * scale, scale, 0x101418);
        run = -1;
      }
    }
  }
}

void stepBullet(Painter& p, int cx, int cy, int n) {
  p.fillCircle(cx, cy, 10, kAccent);
  char b[4];
  snprintf(b, sizeof(b), "%d", n);
  p.text(b, cx, cy + 1, fonts::badge(), kAccentInk, textdatum_t::middle_center);
}

void dots(Painter& p, int cx, int cy, int phase, uint32_t color) {
  for (int i = 0; i < 3; i++) {
    bool on = i == (phase % 3);
    p.fillCircle(cx + (i - 1) * 14, cy, on ? 4 : 3, on ? color : kDivider);
  }
}

int lines(Painter& p, const std::vector<std::string>& ls, int x, int baseline, int lineH,
          const lgfx::IFont* font, uint32_t color, int maxLines) {
  int n = 0;
  for (const auto& l : ls) {
    if (n++ >= maxLines) break;
    p.text(l.c_str(), x, baseline, font, color);
    baseline += lineH;
  }
  return baseline;
}

}  // namespace widgets
}  // namespace ui
